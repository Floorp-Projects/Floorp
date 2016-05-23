// Copyright (c) 2014-2015 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of Google, Inc. nor the names of its contributors
//    may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#ifndef SKIA_EXT_CONVOLVER_LS3_H_
#define SKIA_EXT_CONVOLVER_LS3_H_

#include "convolver.h"

#include <algorithm>

#include "skia/include/core/SkTypes.h"

namespace skia {

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the [begin, end) of the filter.
void ConvolveHorizontally_LS3(const unsigned char* src_data,
                               const ConvolutionFilter1D& filter,
                               unsigned char* out_row);

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the [begin, end) of the filter.
// Process one pixel at a time.
void ConvolveHorizontally1_LS3(const unsigned char* src_data,
                               const ConvolutionFilter1D& filter,
                               unsigned char* out_row);

// Convolves horizontally along four rows. The row data is given in
// |src_data| and continues for the [begin, end) of the filter.
// The algorithm is almost same as |ConvolveHorizontally_LS3|. Please
// refer to that function for detailed comments.
void ConvolveHorizontally4_LS3(const unsigned char* src_data[4],
                                const ConvolutionFilter1D& filter,
                                unsigned char* out_row[4]);

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |source_data_rows| array, with each row
// being |pixel_width| wide.
//
// The output must have room for |pixel_width * 4| bytes.
void ConvolveVertically_LS3(const ConvolutionFilter1D::Fixed* filter_values,
                             int filter_length,
                             unsigned char* const* source_data_rows,
                             int pixel_width,
                             unsigned char* out_row, bool has_alpha);

}  // namespace skia

#endif  // SKIA_EXT_CONVOLVER_LS3_H_
