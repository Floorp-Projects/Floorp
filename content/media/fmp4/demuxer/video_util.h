// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_UTIL_H_
#define MEDIA_BASE_VIDEO_UTIL_H_

#include "mp4_demuxer/basictypes.h"

namespace mp4_demuxer {

class VideoFrame;

// Computes the size of |visible_size| for a given aspect ratio.
IntSize GetNaturalSize(const IntSize& visible_size,
                                    int aspect_ratio_numerator,
                                    int aspect_ratio_denominator);
/*
// Copies a plane of YUV(A) source into a VideoFrame object, taking into account
// source and destinations dimensions.
//
// NOTE: rows is *not* the same as height!
void CopyYPlane(const uint8_t* source, int stride, int rows,
                             VideoFrame* frame);
void CopyUPlane(const uint8_t* source, int stride, int rows,
                             VideoFrame* frame);
void CopyVPlane(const uint8_t* source, int stride, int rows,
                             VideoFrame* frame);
void CopyAPlane(const uint8_t* source, int stride, int rows,
                             VideoFrame* frame);

// Sets alpha plane values to be completely opaque (all 255's).
void MakeOpaqueAPlane(int stride, int rows, VideoFrame* frame);

// |plane| is one of VideoFrame::kYPlane, VideoFrame::kUPlane,
// VideoFrame::kVPlane or VideoFrame::kAPlane
void CopyPlane(size_t plane, const uint8_t* source, int stride,
                            int rows, VideoFrame* frame);


// Fills |frame| containing YUV data to the given color values.
void FillYUV(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v);

// Creates a border in |frame| such that all pixels outside of
// |view_area| are black. The size and position of |view_area|
// must be even to align correctly with the color planes.
// Only YV12 format video frames are currently supported.
void LetterboxYUV(VideoFrame* frame,
                               const gfx::Rect& view_area);

// Rotates |src| plane by |rotation| degree with possible flipping vertically
// and horizontally.
// |rotation| is limited to {0, 90, 180, 270}.
// |width| and |height| are expected to be even numbers.
// Both |src| and |dest| planes are packed and have same |width| and |height|.
// When |width| != |height| and rotated by 90/270, only the maximum square
// portion located in the center is rotated. For example, for width=640 and
// height=480, the rotated area is 480x480 located from row 0 through 479 and
// from column 80 through 559. The leftmost and rightmost 80 columns are
// ignored for both |src| and |dest|.
// The caller is responsible for blanking out the margin area.
void RotatePlaneByPixels(
    const uint8_t* src,
    uint8_t* dest,
    int width,
    int height,
    int rotation,  // Clockwise.
    bool flip_vert,
    bool flip_horiz);

// Return the largest centered rectangle with the same aspect ratio of |content|
// that fits entirely inside of |bounds|.
gfx::Rect ComputeLetterboxRegion(const gfx::Rect& bounds,
                                              const IntSize& content);

// Copy an RGB bitmap into the specified |region_in_frame| of a YUV video frame.
// Fills the regions outside |region_in_frame| with black.
void CopyRGBToVideoFrame(const uint8_t* source,
                                      int stride,
                                      const gfx::Rect& region_in_frame,
                                      VideoFrame* frame);
*/

}  // namespace mp4_demuxer

#endif  // MEDIA_BASE_VIDEO_UTIL_H_
