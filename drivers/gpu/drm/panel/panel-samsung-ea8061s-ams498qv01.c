// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022
 * Author: linux-mdss-dsi-panel-driver-generator from vendor device tree
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct ea8061s_ams498qv01 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	bool prepared;
};

static inline
struct ea8061s_ams498qv01 *to_ea8061s_ams498qv01(struct drm_panel *panel)
{
	return container_of(panel, struct ea8061s_ams498qv01, panel);
}

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static int ea8061s_ams498qv01_on(struct ea8061s_ams498qv01 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_dcs_write_seq(dsi, 0xba, 0xc4);
	dsi_dcs_write_seq(dsi, 0xb2, 0x00, 0x00, 0x00, 0x0a);
	dsi_dcs_write_seq(dsi, 0xf7, 0x03);
	dsi_dcs_write_seq(dsi, 0xb8, 0x19, 0x00);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(20);

	dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	msleep(120);

	return 0;
}

static int ea8061s_ams498qv01_off(struct ea8061s_ams498qv01 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(100);

	return 0;
}

static int ea8061s_ams498qv01_prepare(struct drm_panel *panel)
{
	struct ea8061s_ams498qv01 *ctx = to_ea8061s_ams498qv01(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = ea8061s_ams498qv01_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int ea8061s_ams498qv01_unprepare(struct drm_panel *panel)
{
	struct ea8061s_ams498qv01 *ctx = to_ea8061s_ams498qv01(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = ea8061s_ams498qv01_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);


	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode ea8061s_ams498qv01_mode = {
	.clock = (540 + 126 + 1 + 43) * (960 + 4 + 1 + 11) * 60 / 1000,
	.hdisplay = 540,
	.hsync_start = 540 + 126,
	.hsync_end = 540 + 126 + 1,
	.htotal = 540 + 126 + 1 + 43,
	.vdisplay = 960,
	.vsync_start = 960 + 4,
	.vsync_end = 960 + 4 + 1,
	.vtotal = 960 + 4 + 1 + 11,
	.width_mm = 62,
	.height_mm = 110,
};

static int ea8061s_ams498qv01_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &ea8061s_ams498qv01_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ea8061s_ams498qv01_panel_funcs = {
	.prepare = ea8061s_ams498qv01_prepare,
	.unprepare = ea8061s_ams498qv01_unprepare,
	.get_modes = ea8061s_ams498qv01_get_modes,
};

static int ea8061s_ams498qv01_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

// TODO: Check if /sys/class/backlight/.../actual_brightness actually returns
// correct values. If not, remove this function.
static int ea8061s_ams498qv01_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness;
}

static const struct backlight_ops ea8061s_ams498qv01_bl_ops = {
	.update_status = ea8061s_ams498qv01_bl_update_status,
	.get_brightness = ea8061s_ams498qv01_bl_get_brightness,
};

static struct backlight_device *
ea8061s_ams498qv01_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 365,
		.max_brightness = 365,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &ea8061s_ams498qv01_bl_ops, &props);
}

static int ea8061s_ams498qv01_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ea8061s_ams498qv01 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &ea8061s_ams498qv01_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ctx->panel.backlight = ea8061s_ams498qv01_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int ea8061s_ams498qv01_remove(struct mipi_dsi_device *dsi)
{
	struct ea8061s_ams498qv01 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id ea8061s_ams498qv01_of_match[] = {
	{ .compatible = "mdss,ea8061s-ams498qv01" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ea8061s_ams498qv01_of_match);

static struct mipi_dsi_driver ea8061s_ams498qv01_driver = {
	.probe = ea8061s_ams498qv01_probe,
	.remove = ea8061s_ams498qv01_remove,
	.driver = {
		.name = "panel-ea8061s-ams498qv01",
		.of_match_table = ea8061s_ams498qv01_of_match,
	},
};
module_mipi_dsi_driver(ea8061s_ams498qv01_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ss_dsi_panel_EA8061S_AMS498QV01_QHD");
MODULE_LICENSE("GPL v2");
