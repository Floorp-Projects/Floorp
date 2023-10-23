/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WPF_GPU_RASTER_H
#define MOZILLA_GFX_WPF_GPU_RASTER_H

#include <stddef.h>
#include <stdint.h>

namespace WGR {

enum class FillMode { EvenOdd, Winding };
struct PathBuilder;
struct Point {
  int32_t x;
  int32_t y;
};
constexpr uint8_t PathPointTypeStart = 0;
constexpr uint8_t PathPointTypeLine = 1;
constexpr uint8_t PathPointTypeBezier = 3;
constexpr uint8_t PathPointTypePathTypeMask = 0x07;
constexpr uint8_t PathPointTypeCloseSubpath = 0x80;
struct Path {
  FillMode fill_mode;
  const Point* points;
  size_t num_points;
  const uint8_t* types;
  size_t num_types;
};
struct OutputVertex {
  float x;
  float y;
  float coverage;
};
struct VertexBuffer {
  OutputVertex* data;
  size_t len;
};

extern "C" {
PathBuilder* wgr_new_builder();
void wgr_builder_reset(PathBuilder* pb);
void wgr_builder_move_to(PathBuilder* pb, float x, float y);
void wgr_builder_line_to(PathBuilder* pb, float x, float y);
void wgr_builder_curve_to(PathBuilder* pb, float c1x, float c1y, float c2x,
                          float c2y, float x, float y);
void wgr_builder_quad_to(PathBuilder* pb, float cx, float cy, float x, float y);
void wgr_builder_close(PathBuilder* pb);
void wgr_builder_set_fill_mode(PathBuilder* pb, FillMode fill_mode);
Path wgr_builder_get_path(PathBuilder* pb);
VertexBuffer wgr_path_rasterize_to_tri_list(
    const Path* p, int32_t clip_x, int32_t clip_y, int32_t clip_width,
    int32_t clip_height, bool need_inside = true, bool need_outside = false,
    bool rasterization_truncates = false, OutputVertex* output_ptr = nullptr,
    size_t output_capacity = 0);
void wgr_path_release(Path p);
void wgr_vertex_buffer_release(VertexBuffer vb);
void wgr_builder_release(PathBuilder* pb);
};

}  // namespace WGR

#endif  // MOZILLA_GFX_WPF_GPU_RASTER_H
