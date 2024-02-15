/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_AA_STROKE_H
#define MOZILLA_GFX_AA_STROKE_H

#include <stddef.h>

namespace AAStroke {

enum class LineCap { Round, Square, Butt };
enum class LineJoin { Round, Miter, Bevel };
struct StrokeStyle {
  float width;
  LineCap cap;
  LineJoin join;
  float miter_limit;
};
struct Stroker;
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
Stroker* aa_stroke_new(const StrokeStyle* style,
                       OutputVertex* output_ptr = nullptr,
                       size_t output_capacity = 0);
void aa_stroke_move_to(Stroker* s, float x, float y, bool closed);
void aa_stroke_line_to(Stroker* s, float x, float y, bool end);
void aa_stroke_curve_to(Stroker* s, float c1x, float c1y, float c2x, float c2y,
                        float x, float y, bool end);
void aa_stroke_close(Stroker* s);
VertexBuffer aa_stroke_finish(Stroker* s);
VertexBuffer aa_stroke_filled_circle(float cx, float cy, float radius,
                                     OutputVertex* output_ptr = nullptr,
                                     size_t output_capacity = 0);
void aa_stroke_vertex_buffer_release(VertexBuffer vb);
void aa_stroke_release(Stroker* s);
};

}  // namespace AAStroke

#endif  // MOZILLA_GFX_AA_STROKE_H
