// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
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

#include "2D.h"
#include "convolver.h"

#include <algorithm>

#include "skia/SkTypes.h"


#if defined(USE_SSE2)
#include "convolverSSE2.h"
#endif

#if defined(_MIPS_ARCH_LOONGSON3A)
#include "convolverLS3.h"
#endif

using mozilla::gfx::Factory;

#if defined(SK_CPU_LENDIAN)
#define R_OFFSET_IDX 0
#define G_OFFSET_IDX 1
#define B_OFFSET_IDX 2
#define A_OFFSET_IDX 3
#else
#define R_OFFSET_IDX 3
#define G_OFFSET_IDX 2
#define B_OFFSET_IDX 1
#define A_OFFSET_IDX 0
#endif

#if defined(USE_SSE2)
#define ConvolveHorizontally4_SIMD ConvolveHorizontally4_SSE2
#define ConvolveHorizontally_SIMD ConvolveHorizontally_SSE2
#define ConvolveVertically_SIMD ConvolveVertically_SSE2
#endif

#if defined(_MIPS_ARCH_LOONGSON3A)
#define ConvolveHorizontally4_SIMD ConvolveHorizontally4_LS3
#define ConvolveHorizontally_SIMD ConvolveHorizontally_LS3
#define ConvolveVertically_SIMD ConvolveVertically_LS3
#endif

namespace skia {

namespace {

// Converts the argument to an 8-bit unsigned value by clamping to the range
// 0-255.
inline unsigned char ClampTo8(int a) {
  if (static_cast<unsigned>(a) < 256)
    return a;  // Avoid the extra check in the common case.
  if (a < 0)
    return 0;
  return 255;
}

// Stores a list of rows in a circular buffer. The usage is you write into it
// by calling AdvanceRow. It will keep track of which row in the buffer it
// should use next, and the total number of rows added.
class CircularRowBuffer {
 public:
  // The number of pixels in each row is given in |source_row_pixel_width|.
  // The maximum number of rows needed in the buffer is |max_y_filter_size|
  // (we only need to store enough rows for the biggest filter).
  //
  // We use the |first_input_row| to compute the coordinates of all of the
  // following rows returned by Advance().
  CircularRowBuffer(int dest_row_pixel_width, int max_y_filter_size,
                    int first_input_row)
      : row_byte_width_(dest_row_pixel_width * 4),
        num_rows_(max_y_filter_size),
        next_row_(0),
        next_row_coordinate_(first_input_row) {
    buffer_.resize(row_byte_width_ * max_y_filter_size);
    row_addresses_.resize(num_rows_);
  }

  // Moves to the next row in the buffer, returning a pointer to the beginning
  // of it.
  unsigned char* AdvanceRow() {
    unsigned char* row = &buffer_[next_row_ * row_byte_width_];
    next_row_coordinate_++;

    // Set the pointer to the next row to use, wrapping around if necessary.
    next_row_++;
    if (next_row_ == num_rows_)
      next_row_ = 0;
    return row;
  }

  // Returns a pointer to an "unrolled" array of rows. These rows will start
  // at the y coordinate placed into |*first_row_index| and will continue in
  // order for the maximum number of rows in this circular buffer.
  //
  // The |first_row_index_| may be negative. This means the circular buffer
  // starts before the top of the image (it hasn't been filled yet).
  unsigned char* const* GetRowAddresses(int* first_row_index) {
    // Example for a 4-element circular buffer holding coords 6-9.
    //   Row 0   Coord 8
    //   Row 1   Coord 9
    //   Row 2   Coord 6  <- next_row_ = 2, next_row_coordinate_ = 10.
    //   Row 3   Coord 7
    //
    // The "next" row is also the first (lowest) coordinate. This computation
    // may yield a negative value, but that's OK, the math will work out
    // since the user of this buffer will compute the offset relative
    // to the first_row_index and the negative rows will never be used.
    *first_row_index = next_row_coordinate_ - num_rows_;

    int cur_row = next_row_;
    for (int i = 0; i < num_rows_; i++) {
      row_addresses_[i] = &buffer_[cur_row * row_byte_width_];

      // Advance to the next row, wrapping if necessary.
      cur_row++;
      if (cur_row == num_rows_)
        cur_row = 0;
    }
    return &row_addresses_[0];
  }

 private:
  // The buffer storing the rows. They are packed, each one row_byte_width_.
  std::vector<unsigned char> buffer_;

  // Number of bytes per row in the |buffer_|.
  int row_byte_width_;

  // The number of rows available in the buffer.
  int num_rows_;

  // The next row index we should write into. This wraps around as the
  // circular buffer is used.
  int next_row_;

  // The y coordinate of the |next_row_|. This is incremented each time a
  // new row is appended and does not wrap.
  int next_row_coordinate_;

  // Buffer used by GetRowAddresses().
  std::vector<unsigned char*> row_addresses_;
};

}  // namespace

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the [begin, end) of the filter.
template<bool has_alpha>
void ConvolveHorizontally(const unsigned char* src_data,
                          int begin, int end,
                          const ConvolutionFilter1D& filter,
                          unsigned char* out_row) {
  // Loop over each pixel on this row in the output image.
  for (int out_x = begin; out_x < end; out_x++) {
    // Get the filter that determines the current output pixel.
    int filter_offset, filter_length;
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const unsigned char* row_to_filter = &src_data[filter_offset * 4];

    // Apply the filter to the row to get the destination pixel in |accum|.
    int accum[4] = {0};
    for (int filter_x = 0; filter_x < filter_length; filter_x++) {
      ConvolutionFilter1D::Fixed cur_filter = filter_values[filter_x];
      accum[0] += cur_filter * row_to_filter[filter_x * 4 + R_OFFSET_IDX];
      accum[1] += cur_filter * row_to_filter[filter_x * 4 + G_OFFSET_IDX];
      accum[2] += cur_filter * row_to_filter[filter_x * 4 + B_OFFSET_IDX];
      if (has_alpha)
        accum[3] += cur_filter * row_to_filter[filter_x * 4 + A_OFFSET_IDX];
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of fractional part.
    accum[0] >>= ConvolutionFilter1D::kShiftBits;
    accum[1] >>= ConvolutionFilter1D::kShiftBits;
    accum[2] >>= ConvolutionFilter1D::kShiftBits;
    if (has_alpha)
      accum[3] >>= ConvolutionFilter1D::kShiftBits;

    // Store the new pixel.
    out_row[out_x * 4 + R_OFFSET_IDX] = ClampTo8(accum[0]);
    out_row[out_x * 4 + G_OFFSET_IDX] = ClampTo8(accum[1]);
    out_row[out_x * 4 + B_OFFSET_IDX] = ClampTo8(accum[2]);
    if (has_alpha)
      out_row[out_x * 4 + A_OFFSET_IDX] = ClampTo8(accum[3]);
  }
}

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |source_data_rows| array, with each row
// being |end - begin| wide.
//
// The output must have room for |(end - begin) * 4| bytes.
template<bool has_alpha>
void ConvolveVertically(const ConvolutionFilter1D::Fixed* filter_values,
                        int filter_length,
                        unsigned char* const* source_data_rows,
                        int begin, int end, unsigned char* out_row) {
  // We go through each column in the output and do a vertical convolution,
  // generating one output pixel each time.
  for (int out_x = begin; out_x < end; out_x++) {
    // Compute the number of bytes over in each row that the current column
    // we're convolving starts at. The pixel will cover the next 4 bytes.
    int byte_offset = out_x * 4;

    // Apply the filter to one column of pixels.
    int accum[4] = {0};
    for (int filter_y = 0; filter_y < filter_length; filter_y++) {
      ConvolutionFilter1D::Fixed cur_filter = filter_values[filter_y];
      accum[0] += cur_filter 
	* source_data_rows[filter_y][byte_offset + R_OFFSET_IDX];
      accum[1] += cur_filter 
	* source_data_rows[filter_y][byte_offset + G_OFFSET_IDX];
      accum[2] += cur_filter 
	* source_data_rows[filter_y][byte_offset + B_OFFSET_IDX];
      if (has_alpha)
        accum[3] += cur_filter 
	  * source_data_rows[filter_y][byte_offset + A_OFFSET_IDX];
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of precision.
    accum[0] >>= ConvolutionFilter1D::kShiftBits;
    accum[1] >>= ConvolutionFilter1D::kShiftBits;
    accum[2] >>= ConvolutionFilter1D::kShiftBits;
    if (has_alpha)
      accum[3] >>= ConvolutionFilter1D::kShiftBits;

    // Store the new pixel.
    out_row[byte_offset + R_OFFSET_IDX] = ClampTo8(accum[0]);
    out_row[byte_offset + G_OFFSET_IDX] = ClampTo8(accum[1]);
    out_row[byte_offset + B_OFFSET_IDX] = ClampTo8(accum[2]);
    if (has_alpha) {
      unsigned char alpha = ClampTo8(accum[3]);

      // Make sure the alpha channel doesn't come out smaller than any of the
      // color channels. We use premultipled alpha channels, so this should
      // never happen, but rounding errors will cause this from time to time.
      // These "impossible" colors will cause overflows (and hence random pixel
      // values) when the resulting bitmap is drawn to the screen.
      //
      // We only need to do this when generating the final output row (here).
      int max_color_channel = std::max(out_row[byte_offset + R_OFFSET_IDX],
          std::max(out_row[byte_offset + G_OFFSET_IDX], out_row[byte_offset + B_OFFSET_IDX]));
      if (alpha < max_color_channel)
        out_row[byte_offset + A_OFFSET_IDX] = max_color_channel;
      else
        out_row[byte_offset + A_OFFSET_IDX] = alpha;
    } else {
      // No alpha channel, the image is opaque.
      out_row[byte_offset + A_OFFSET_IDX] = 0xff;
    }
  }
}

void ConvolveVertically(const ConvolutionFilter1D::Fixed* filter_values,
                        int filter_length,
                        unsigned char* const* source_data_rows,
                        int width, unsigned char* out_row,
                        bool has_alpha, bool use_simd) {
  int processed = 0;

#if defined(USE_SSE2) || defined(_MIPS_ARCH_LOONGSON3A)
  // If the binary was not built with SSE2 support, we had to fallback to C version.
  int simd_width = width & ~3;
  if (use_simd && simd_width) {
    ConvolveVertically_SIMD(filter_values, filter_length,
                            source_data_rows, 0, simd_width,
                            out_row, has_alpha);
    processed = simd_width;
  }
#endif
    
  if (width > processed) {
    if (has_alpha) {
      ConvolveVertically<true>(filter_values, filter_length, source_data_rows,
                               processed, width, out_row);
    } else {
      ConvolveVertically<false>(filter_values, filter_length, source_data_rows,
                                processed, width, out_row);
    }
  }
}

void ConvolveHorizontally(const unsigned char* src_data,
                          const ConvolutionFilter1D& filter,
                          unsigned char* out_row,
                          bool has_alpha, bool use_simd) {
  int width = filter.num_values();
  int processed = 0;
#if defined(USE_SSE2)
  int simd_width = width & ~3;
  if (use_simd && simd_width) {
    // SIMD implementation works with 4 pixels at a time.
    // Therefore we process as much as we can using SSE and then use
    // C implementation for leftovers
    ConvolveHorizontally_SSE2(src_data, 0, simd_width, filter, out_row);
    processed = simd_width;
  }
#endif

  if (width > processed) {
    if (has_alpha) {
      ConvolveHorizontally<true>(src_data, processed, width, filter, out_row);
    } else {
      ConvolveHorizontally<false>(src_data, processed, width, filter, out_row);
    }
  }
}

// ConvolutionFilter1D ---------------------------------------------------------

ConvolutionFilter1D::ConvolutionFilter1D()
    : max_filter_(0) {
}

ConvolutionFilter1D::~ConvolutionFilter1D() {
}

void ConvolutionFilter1D::AddFilter(int filter_offset,
                                    const float* filter_values,
                                    int filter_length) {
  SkASSERT(filter_length > 0);

  std::vector<Fixed> fixed_values;
  fixed_values.reserve(filter_length);

  for (int i = 0; i < filter_length; ++i)
    fixed_values.push_back(FloatToFixed(filter_values[i]));

  AddFilter(filter_offset, &fixed_values[0], filter_length);
}

void ConvolutionFilter1D::AddFilter(int filter_offset,
                                    const Fixed* filter_values,
                                    int filter_length) {
  // It is common for leading/trailing filter values to be zeros. In such
  // cases it is beneficial to only store the central factors.
  // For a scaling to 1/4th in each dimension using a Lanczos-2 filter on
  // a 1080p image this optimization gives a ~10% speed improvement.
  int first_non_zero = 0;
  while (first_non_zero < filter_length && filter_values[first_non_zero] == 0)
    first_non_zero++;

  if (first_non_zero < filter_length) {
    // Here we have at least one non-zero factor.
    int last_non_zero = filter_length - 1;
    while (last_non_zero >= 0 && filter_values[last_non_zero] == 0)
      last_non_zero--;

    filter_offset += first_non_zero;
    filter_length = last_non_zero + 1 - first_non_zero;
    SkASSERT(filter_length > 0);

    for (int i = first_non_zero; i <= last_non_zero; i++)
      filter_values_.push_back(filter_values[i]);
  } else {
    // Here all the factors were zeroes.
    filter_length = 0;
  }

  FilterInstance instance;

  // We pushed filter_length elements onto filter_values_
  instance.data_location = (static_cast<int>(filter_values_.size()) -
                            filter_length);
  instance.offset = filter_offset;
  instance.length = filter_length;
  filters_.push_back(instance);

  max_filter_ = std::max(max_filter_, filter_length);
}

void BGRAConvolve2D(const unsigned char* source_data,
                    int source_byte_row_stride,
                    bool source_has_alpha,
                    const ConvolutionFilter1D& filter_x,
                    const ConvolutionFilter1D& filter_y,
                    int output_byte_row_stride,
                    unsigned char* output) {
  bool use_simd = Factory::HasSSE2();

#if !defined(USE_SSE2)
  // Even we have runtime support for SSE2 instructions, since the binary
  // was not built with SSE2 support, we had to fallback to C version.
  use_simd = false;
#endif

#if defined(_MIPS_ARCH_LOONGSON3A)
  use_simd = true;
#endif


  int max_y_filter_size = filter_y.max_filter();

  // The next row in the input that we will generate a horizontally
  // convolved row for. If the filter doesn't start at the beginning of the
  // image (this is the case when we are only resizing a subset), then we
  // don't want to generate any output rows before that. Compute the starting
  // row for convolution as the first pixel for the first vertical filter.
  int filter_offset, filter_length;
  const ConvolutionFilter1D::Fixed* filter_values =
      filter_y.FilterForValue(0, &filter_offset, &filter_length);
  int next_x_row = filter_offset;

  // We loop over each row in the input doing a horizontal convolution. This
  // will result in a horizontally convolved image. We write the results into
  // a circular buffer of convolved rows and do vertical convolution as rows
  // are available. This prevents us from having to store the entire
  // intermediate image and helps cache coherency.
  // We will need four extra rows to allow horizontal convolution could be done
  // simultaneously. We also padding each row in row buffer to be aligned-up to
  // 16 bytes.
  // TODO(jiesun): We do not use aligned load from row buffer in vertical
  // convolution pass yet. Somehow Windows does not like it.
  int row_buffer_width = (filter_x.num_values() + 15) & ~0xF;
  int row_buffer_height = max_y_filter_size + (use_simd ? 4 : 0);
  CircularRowBuffer row_buffer(row_buffer_width,
                               row_buffer_height,
                               filter_offset);

  // Loop over every possible output row, processing just enough horizontal
  // convolutions to run each subsequent vertical convolution.
  SkASSERT(output_byte_row_stride >= filter_x.num_values() * 4);
  int num_output_rows = filter_y.num_values();
  int pixel_width = filter_x.num_values();

  // We need to check which is the last line to convolve before we advance 4
  // lines in one iteration.
  int last_filter_offset, last_filter_length;
  filter_y.FilterForValue(num_output_rows - 1, &last_filter_offset,
                          &last_filter_length);

  for (int out_y = 0; out_y < num_output_rows; out_y++) {
    filter_values = filter_y.FilterForValue(out_y,
                                            &filter_offset, &filter_length);

    // Generate output rows until we have enough to run the current filter.
    if (use_simd) {
#if defined(USE_SSE2) || defined(_MIPS_ARCH_LOONGSON3A)
      // We don't want to process too much rows in batches of 4 because
      // we can go out-of-bounds at the end
      while (next_x_row < filter_offset + filter_length) {
        if (next_x_row + 3 < last_filter_offset + last_filter_length - 3) {
          const unsigned char* src[4];
          unsigned char* out_row[4];
          for (int i = 0; i < 4; ++i) {
            src[i] = &source_data[(next_x_row + i) * source_byte_row_stride];
            out_row[i] = row_buffer.AdvanceRow();
          }
          ConvolveHorizontally4_SIMD(src, 0, pixel_width, filter_x, out_row);
          next_x_row += 4;
        } else {
          unsigned char* buffer = row_buffer.AdvanceRow();

          // For last rows, SSE2 load possibly to access data beyond the
          // image area. therefore we use cobined C+SSE version here
          int simd_width = pixel_width & ~3;
          if (simd_width) {
            ConvolveHorizontally_SIMD(
                &source_data[next_x_row * source_byte_row_stride],
                0, simd_width, filter_x, buffer);
          }

          if (pixel_width > simd_width) {
            if (source_has_alpha) {
              ConvolveHorizontally<true>(
                  &source_data[next_x_row * source_byte_row_stride],
                  simd_width, pixel_width, filter_x, buffer);
            } else {
              ConvolveHorizontally<false>(
                  &source_data[next_x_row * source_byte_row_stride],
                  simd_width, pixel_width, filter_x, buffer);
            }
          }
          next_x_row++;
        }
      }
#endif
    } else {
      while (next_x_row < filter_offset + filter_length) {
        if (source_has_alpha) {
          ConvolveHorizontally<true>(
              &source_data[next_x_row * source_byte_row_stride],
              0, pixel_width, filter_x, row_buffer.AdvanceRow());
        } else {
          ConvolveHorizontally<false>(
              &source_data[next_x_row * source_byte_row_stride],
              0, pixel_width, filter_x, row_buffer.AdvanceRow());
        }
        next_x_row++;
      }
    }

    // Compute where in the output image this row of final data will go.
    unsigned char* cur_output_row = &output[out_y * output_byte_row_stride];

    // Get the list of rows that the circular buffer has, in order.
    int first_row_in_circular_buffer;
    unsigned char* const* rows_to_convolve =
        row_buffer.GetRowAddresses(&first_row_in_circular_buffer);

    // Now compute the start of the subset of those rows that the filter
    // needs.
    unsigned char* const* first_row_for_filter =
        &rows_to_convolve[filter_offset - first_row_in_circular_buffer];

    ConvolveVertically(filter_values, filter_length,
                       first_row_for_filter, pixel_width,
                       cur_output_row, source_has_alpha, use_simd);
  }
}

}  // namespace skia
