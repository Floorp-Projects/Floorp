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

#include "convolver.h"

#include <algorithm>
#include "nsAlgorithm.h"

#include "skia/SkTypes.h"

// note: SIMD_SSE2 is not enabled because of bugs, apparently

#if defined(SIMD_SSE2)
#include <emmintrin.h>  // ARCH_CPU_X86_FAMILY was defined in build/config.h
#endif

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

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the num_values() of the filter.
template<bool has_alpha>
void ConvolveHorizontally(const unsigned char* src_data,
                          const ConvolutionFilter1D& filter,
                          unsigned char* out_row) {
  // Loop over each pixel on this row in the output image.
  int num_values = filter.num_values();
  for (int out_x = 0; out_x < num_values; out_x++) {
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
// being |pixel_width| wide.
//
// The output must have room for |pixel_width * 4| bytes.
template<bool has_alpha>
void ConvolveVertically(const ConvolutionFilter1D::Fixed* filter_values,
                        int filter_length,
                        unsigned char* const* source_data_rows,
                        int pixel_width,
                        unsigned char* out_row) {
  // We go through each column in the output and do a vertical convolution,
  // generating one output pixel each time.
  for (int out_x = 0; out_x < pixel_width; out_x++) {
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
      int max_color_channel = NS_MAX(out_row[byte_offset + R_OFFSET_IDX],
          NS_MAX(out_row[byte_offset + G_OFFSET_IDX], out_row[byte_offset + B_OFFSET_IDX]));
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


// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the num_values() of the filter.
void ConvolveHorizontally_SSE2(const unsigned char* src_data,
                               const ConvolutionFilter1D& filter,
                               unsigned char* out_row) {
#if defined(SIMD_SSE2)
  int num_values = filter.num_values();

  int filter_offset, filter_length;
  __m128i zero = _mm_setzero_si128();
  __m128i mask[4];
  // |mask| will be used to decimate all extra filter coefficients that are
  // loaded by SIMD when |filter_length| is not divisible by 4.
  // mask[0] is not used in following algorithm.
  mask[1] = _mm_set_epi16(0, 0, 0, 0, 0, 0, 0, -1);
  mask[2] = _mm_set_epi16(0, 0, 0, 0, 0, 0, -1, -1);
  mask[3] = _mm_set_epi16(0, 0, 0, 0, 0, -1, -1, -1);

  // Output one pixel each iteration, calculating all channels (RGBA) together.
  for (int out_x = 0; out_x < num_values; out_x++) {
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    __m128i accum = _mm_setzero_si128();

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const __m128i* row_to_filter =
        reinterpret_cast<const __m128i*>(&src_data[filter_offset << 2]);

    // We will load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < filter_length >> 2; filter_x++) {

      // Load 4 coefficients => duplicate 1st and 2nd of them for all channels.
      __m128i coeff, coeff16;
      // [16] xx xx xx xx c3 c2 c1 c0
      coeff = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // [16] xx xx xx xx c1 c1 c0 c0
      coeff16 = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(1, 1, 0, 0));
      // [16] c1 c1 c1 c1 c0 c0 c0 c0
      coeff16 = _mm_unpacklo_epi16(coeff16, coeff16);

      // Load four pixels => unpack the first two pixels to 16 bits =>
      // multiply with coefficients => accumulate the convolution result.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src8 = _mm_loadu_si128(row_to_filter);
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32]  a0*c0 b0*c0 g0*c0 r0*c0
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);
      // [32]  a1*c1 b1*c1 g1*c1 r1*c1
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);

      // Duplicate 3rd and 4th coefficients for all channels =>
      // unpack the 3rd and 4th pixels to 16 bits => multiply with coefficients
      // => accumulate the convolution results.
      // [16] xx xx xx xx c3 c3 c2 c2
      coeff16 = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(3, 3, 2, 2));
      // [16] c3 c3 c3 c3 c2 c2 c2 c2
      coeff16 = _mm_unpacklo_epi16(coeff16, coeff16);
      // [16] a3 g3 b3 r3 a2 g2 b2 r2
      src16 = _mm_unpackhi_epi8(src8, zero);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32]  a2*c2 b2*c2 g2*c2 r2*c2
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);
      // [32]  a3*c3 b3*c3 g3*c3 r3*c3
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);

      // Advance the pixel and coefficients pointers.
      row_to_filter += 1;
      filter_values += 4;
    }

    // When |filter_length| is not divisible by 4, we need to decimate some of
    // the filter coefficient that was loaded incorrectly to zero; Other than
    // that the algorithm is same with above, exceot that the 4th pixel will be
    // always absent.
    int r = filter_length&3;
    if (r) {
      // Note: filter_values must be padded to align_up(filter_offset, 8).
      __m128i coeff, coeff16;
      coeff = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // Mask out extra filter taps.
      coeff = _mm_and_si128(coeff, mask[r]);
      coeff16 = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(1, 1, 0, 0));
      coeff16 = _mm_unpacklo_epi16(coeff16, coeff16);

      // Note: line buffer must be padded to align_up(filter_offset, 16).
      // We resolve this by use C-version for the last horizontal line.
      __m128i src8 = _mm_loadu_si128(row_to_filter);
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);

      src16 = _mm_unpackhi_epi8(src8, zero);
      coeff16 = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(3, 3, 2, 2));
      coeff16 = _mm_unpacklo_epi16(coeff16, coeff16);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum = _mm_add_epi32(accum, t);
    }

    // Shift right for fixed point implementation.
    accum = _mm_srai_epi32(accum, ConvolutionFilter1D::kShiftBits);

    // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
    accum = _mm_packs_epi32(accum, zero);
    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    accum = _mm_packus_epi16(accum, zero);

    // Store the pixel value of 32 bits.
    *(reinterpret_cast<int*>(out_row)) = _mm_cvtsi128_si32(accum);
    out_row += 4;
  }
#endif
}

// Convolves horizontally along four rows. The row data is given in
// |src_data| and continues for the num_values() of the filter.
// The algorithm is almost same as |ConvolveHorizontally_SSE2|. Please
// refer to that function for detailed comments.
void ConvolveHorizontally4_SSE2(const unsigned char* src_data[4],
                                const ConvolutionFilter1D& filter,
                                unsigned char* out_row[4]) {
#if defined(SIMD_SSE2)
  int num_values = filter.num_values();

  int filter_offset, filter_length;
  __m128i zero = _mm_setzero_si128();
  __m128i mask[4];
  // |mask| will be used to decimate all extra filter coefficients that are
  // loaded by SIMD when |filter_length| is not divisible by 4.
  // mask[0] is not used in following algorithm.
  mask[1] = _mm_set_epi16(0, 0, 0, 0, 0, 0, 0, -1);
  mask[2] = _mm_set_epi16(0, 0, 0, 0, 0, 0, -1, -1);
  mask[3] = _mm_set_epi16(0, 0, 0, 0, 0, -1, -1, -1);

  // Output one pixel each iteration, calculating all channels (RGBA) together.
  for (int out_x = 0; out_x < num_values; out_x++) {
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // four pixels in a column per iteration.
    __m128i accum0 = _mm_setzero_si128();
    __m128i accum1 = _mm_setzero_si128();
    __m128i accum2 = _mm_setzero_si128();
    __m128i accum3 = _mm_setzero_si128();
    int start = (filter_offset<<2);
    // We will load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < (filter_length >> 2); filter_x++) {
      __m128i coeff, coeff16lo, coeff16hi;
      // [16] xx xx xx xx c3 c2 c1 c0
      coeff = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // [16] xx xx xx xx c1 c1 c0 c0
      coeff16lo = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(1, 1, 0, 0));
      // [16] c1 c1 c1 c1 c0 c0 c0 c0
      coeff16lo = _mm_unpacklo_epi16(coeff16lo, coeff16lo);
      // [16] xx xx xx xx c3 c3 c2 c2
      coeff16hi = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(3, 3, 2, 2));
      // [16] c3 c3 c3 c3 c2 c2 c2 c2
      coeff16hi = _mm_unpacklo_epi16(coeff16hi, coeff16hi);

      __m128i src8, src16, mul_hi, mul_lo, t;

#define ITERATION(src, accum)                                          \
      src8 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));   \
      src16 = _mm_unpacklo_epi8(src8, zero);                           \
      mul_hi = _mm_mulhi_epi16(src16, coeff16lo);                      \
      mul_lo = _mm_mullo_epi16(src16, coeff16lo);                      \
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);                          \
      accum = _mm_add_epi32(accum, t);                                 \
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);                          \
      accum = _mm_add_epi32(accum, t);                                 \
      src16 = _mm_unpackhi_epi8(src8, zero);                           \
      mul_hi = _mm_mulhi_epi16(src16, coeff16hi);                      \
      mul_lo = _mm_mullo_epi16(src16, coeff16hi);                      \
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);                          \
      accum = _mm_add_epi32(accum, t);                                 \
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);                          \
      accum = _mm_add_epi32(accum, t)

      ITERATION(src_data[0] + start, accum0);
      ITERATION(src_data[1] + start, accum1);
      ITERATION(src_data[2] + start, accum2);
      ITERATION(src_data[3] + start, accum3);

      start += 16;
      filter_values += 4;
    }

    int r = filter_length & 3;
    if (r) {
      // Note: filter_values must be padded to align_up(filter_offset, 8);
      __m128i coeff;
      coeff = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // Mask out extra filter taps.
      coeff = _mm_and_si128(coeff, mask[r]);

      __m128i coeff16lo = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(1, 1, 0, 0));
      /* c1 c1 c1 c1 c0 c0 c0 c0 */
      coeff16lo = _mm_unpacklo_epi16(coeff16lo, coeff16lo);
      __m128i coeff16hi = _mm_shufflelo_epi16(coeff, _MM_SHUFFLE(3, 3, 2, 2));
      coeff16hi = _mm_unpacklo_epi16(coeff16hi, coeff16hi);

      __m128i src8, src16, mul_hi, mul_lo, t;

      ITERATION(src_data[0] + start, accum0);
      ITERATION(src_data[1] + start, accum1);
      ITERATION(src_data[2] + start, accum2);
      ITERATION(src_data[3] + start, accum3);
    }

    accum0 = _mm_srai_epi32(accum0, ConvolutionFilter1D::kShiftBits);
    accum0 = _mm_packs_epi32(accum0, zero);
    accum0 = _mm_packus_epi16(accum0, zero);
    accum1 = _mm_srai_epi32(accum1, ConvolutionFilter1D::kShiftBits);
    accum1 = _mm_packs_epi32(accum1, zero);
    accum1 = _mm_packus_epi16(accum1, zero);
    accum2 = _mm_srai_epi32(accum2, ConvolutionFilter1D::kShiftBits);
    accum2 = _mm_packs_epi32(accum2, zero);
    accum2 = _mm_packus_epi16(accum2, zero);
    accum3 = _mm_srai_epi32(accum3, ConvolutionFilter1D::kShiftBits);
    accum3 = _mm_packs_epi32(accum3, zero);
    accum3 = _mm_packus_epi16(accum3, zero);

    *(reinterpret_cast<int*>(out_row[0])) = _mm_cvtsi128_si32(accum0);
    *(reinterpret_cast<int*>(out_row[1])) = _mm_cvtsi128_si32(accum1);
    *(reinterpret_cast<int*>(out_row[2])) = _mm_cvtsi128_si32(accum2);
    *(reinterpret_cast<int*>(out_row[3])) = _mm_cvtsi128_si32(accum3);

    out_row[0] += 4;
    out_row[1] += 4;
    out_row[2] += 4;
    out_row[3] += 4;
  }
#endif
}

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |source_data_rows| array, with each row
// being |pixel_width| wide.
//
// The output must have room for |pixel_width * 4| bytes.
template<bool has_alpha>
void ConvolveVertically_SSE2(const ConvolutionFilter1D::Fixed* filter_values,
                             int filter_length,
                             unsigned char* const* source_data_rows,
                             int pixel_width,
                             unsigned char* out_row) {
#if defined(SIMD_SSE2)
  int width = pixel_width & ~3;

  __m128i zero = _mm_setzero_si128();
  __m128i accum0, accum1, accum2, accum3, coeff16;
  const __m128i* src;
  // Output four pixels per iteration (16 bytes).
  for (int out_x = 0; out_x < width; out_x += 4) {

    // Accumulated result for each pixel. 32 bits per RGBA channel.
    accum0 = _mm_setzero_si128();
    accum1 = _mm_setzero_si128();
    accum2 = _mm_setzero_si128();
    accum3 = _mm_setzero_si128();

    // Convolve with one filter coefficient per iteration.
    for (int filter_y = 0; filter_y < filter_length; filter_y++) {

      // Duplicate the filter coefficient 8 times.
      // [16] cj cj cj cj cj cj cj cj
      coeff16 = _mm_set1_epi16(filter_values[filter_y]);

      // Load four pixels (16 bytes) together.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      src = reinterpret_cast<const __m128i*>(
          &source_data_rows[filter_y][out_x << 2]);
      __m128i src8 = _mm_loadu_si128(src);

      // Unpack 1st and 2nd pixels from 8 bits to 16 bits for each channels =>
      // multiply with current coefficient => accumulate the result.
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a0 b0 g0 r0
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum0 = _mm_add_epi32(accum0, t);
      // [32] a1 b1 g1 r1
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum1 = _mm_add_epi32(accum1, t);

      // Unpack 3rd and 4th pixels from 8 bits to 16 bits for each channels =>
      // multiply with current coefficient => accumulate the result.
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      src16 = _mm_unpackhi_epi8(src8, zero);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a2 b2 g2 r2
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum2 = _mm_add_epi32(accum2, t);
      // [32] a3 b3 g3 r3
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum3 = _mm_add_epi32(accum3, t);
    }

    // Shift right for fixed point implementation.
    accum0 = _mm_srai_epi32(accum0, ConvolutionFilter1D::kShiftBits);
    accum1 = _mm_srai_epi32(accum1, ConvolutionFilter1D::kShiftBits);
    accum2 = _mm_srai_epi32(accum2, ConvolutionFilter1D::kShiftBits);
    accum3 = _mm_srai_epi32(accum3, ConvolutionFilter1D::kShiftBits);

    // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packs_epi32(accum0, accum1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    accum2 = _mm_packs_epi32(accum2, accum3);

    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packus_epi16(accum0, accum2);

    if (has_alpha) {
      // Compute the max(ri, gi, bi) for each pixel.
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      __m128i a = _mm_srli_epi32(accum0, 8);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      __m128i b = _mm_max_epu8(a, accum0);  // Max of r and g.
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = _mm_srli_epi32(accum0, 16);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = _mm_max_epu8(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = _mm_slli_epi32(b, 24);

      // Make sure the value of alpha channel is always larger than maximum
      // value of color channels.
      accum0 = _mm_max_epu8(b, accum0);
    } else {
      // Set value of alpha channels to 0xFF.
      __m128i mask = _mm_set1_epi32(0xff000000);
      accum0 = _mm_or_si128(accum0, mask);
    }

    // Store the convolution result (16 bytes) and advance the pixel pointers.
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out_row), accum0);
    out_row += 16;
  }

  // When the width of the output is not divisible by 4, We need to save one
  // pixel (4 bytes) each time. And also the fourth pixel is always absent.
  if (pixel_width & 3) {
    accum0 = _mm_setzero_si128();
    accum1 = _mm_setzero_si128();
    accum2 = _mm_setzero_si128();
    for (int filter_y = 0; filter_y < filter_length; ++filter_y) {
      coeff16 = _mm_set1_epi16(filter_values[filter_y]);
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      src = reinterpret_cast<const __m128i*>(
          &source_data_rows[filter_y][width<<2]);
      __m128i src8 = _mm_loadu_si128(src);
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = _mm_unpacklo_epi8(src8, zero);
      __m128i mul_hi = _mm_mulhi_epi16(src16, coeff16);
      __m128i mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a0 b0 g0 r0
      __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum0 = _mm_add_epi32(accum0, t);
      // [32] a1 b1 g1 r1
      t = _mm_unpackhi_epi16(mul_lo, mul_hi);
      accum1 = _mm_add_epi32(accum1, t);
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      src16 = _mm_unpackhi_epi8(src8, zero);
      mul_hi = _mm_mulhi_epi16(src16, coeff16);
      mul_lo = _mm_mullo_epi16(src16, coeff16);
      // [32] a2 b2 g2 r2
      t = _mm_unpacklo_epi16(mul_lo, mul_hi);
      accum2 = _mm_add_epi32(accum2, t);
    }

    accum0 = _mm_srai_epi32(accum0, ConvolutionFilter1D::kShiftBits);
    accum1 = _mm_srai_epi32(accum1, ConvolutionFilter1D::kShiftBits);
    accum2 = _mm_srai_epi32(accum2, ConvolutionFilter1D::kShiftBits);
    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packs_epi32(accum0, accum1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    accum2 = _mm_packs_epi32(accum2, zero);
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = _mm_packus_epi16(accum0, accum2);
    if (has_alpha) {
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      __m128i a = _mm_srli_epi32(accum0, 8);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      __m128i b = _mm_max_epu8(a, accum0);  // Max of r and g.
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = _mm_srli_epi32(accum0, 16);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = _mm_max_epu8(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = _mm_slli_epi32(b, 24);
      accum0 = _mm_max_epu8(b, accum0);
    } else {
      __m128i mask = _mm_set1_epi32(0xff000000);
      accum0 = _mm_or_si128(accum0, mask);
    }

    for (int out_x = width; out_x < pixel_width; out_x++) {
      *(reinterpret_cast<int*>(out_row)) = _mm_cvtsi128_si32(accum0);
      accum0 = _mm_srli_si128(accum0, 4);
      out_row += 4;
    }
  }
#endif
}

}  // namespace

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

  max_filter_ = NS_MAX(max_filter_, filter_length);
}

void BGRAConvolve2D(const unsigned char* source_data,
                    int source_byte_row_stride,
                    bool source_has_alpha,
                    const ConvolutionFilter1D& filter_x,
                    const ConvolutionFilter1D& filter_y,
                    int output_byte_row_stride,
                    unsigned char* output,
                    bool use_sse2) {
#if !defined(SIMD_SSE2)
  // Even we have runtime support for SSE2 instructions, since the binary
  // was not built with SSE2 support, we had to fallback to C version.
  use_sse2 = false;
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
  int row_buffer_height = max_y_filter_size + (use_sse2 ? 4 : 0);
  CircularRowBuffer row_buffer(row_buffer_width,
                               row_buffer_height,
                               filter_offset);

  // Loop over every possible output row, processing just enough horizontal
  // convolutions to run each subsequent vertical convolution.
  SkASSERT(output_byte_row_stride >= filter_x.num_values() * 4);
  int num_output_rows = filter_y.num_values();

  // We need to check which is the last line to convolve before we advance 4
  // lines in one iteration.
  int last_filter_offset, last_filter_length;
  filter_y.FilterForValue(num_output_rows - 1, &last_filter_offset,
                          &last_filter_length);

  for (int out_y = 0; out_y < num_output_rows; out_y++) {
    filter_values = filter_y.FilterForValue(out_y,
                                            &filter_offset, &filter_length);

    // Generate output rows until we have enough to run the current filter.
    if (use_sse2) {
      while (next_x_row < filter_offset + filter_length) {
        if (next_x_row + 3 < last_filter_offset + last_filter_length - 1) {
          const unsigned char* src[4];
          unsigned char* out_row[4];
          for (int i = 0; i < 4; ++i) {
            src[i] = &source_data[(next_x_row + i) * source_byte_row_stride];
            out_row[i] = row_buffer.AdvanceRow();
          }
          ConvolveHorizontally4_SSE2(src, filter_x, out_row);
          next_x_row += 4;
        } else {
          // For the last row, SSE2 load possibly to access data beyond the
          // image area. therefore we use C version here. 
          if (next_x_row == last_filter_offset + last_filter_length - 1) {
            if (source_has_alpha) {
              ConvolveHorizontally<true>(
                  &source_data[next_x_row * source_byte_row_stride],
                  filter_x, row_buffer.AdvanceRow());
            } else {
              ConvolveHorizontally<false>(
                  &source_data[next_x_row * source_byte_row_stride],
                  filter_x, row_buffer.AdvanceRow());
            }
          } else {
            ConvolveHorizontally_SSE2(
                &source_data[next_x_row * source_byte_row_stride],
                filter_x, row_buffer.AdvanceRow());
          }
          next_x_row++;
        }
      }
    } else {
      while (next_x_row < filter_offset + filter_length) {
        if (source_has_alpha) {
          ConvolveHorizontally<true>(
              &source_data[next_x_row * source_byte_row_stride],
              filter_x, row_buffer.AdvanceRow());
        } else {
          ConvolveHorizontally<false>(
              &source_data[next_x_row * source_byte_row_stride],
              filter_x, row_buffer.AdvanceRow());
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

    if (source_has_alpha) {
      if (use_sse2) {
        ConvolveVertically_SSE2<true>(filter_values, filter_length,
                                      first_row_for_filter,
                                      filter_x.num_values(), cur_output_row);
      } else {
        ConvolveVertically<true>(filter_values, filter_length,
                                 first_row_for_filter,
                                 filter_x.num_values(), cur_output_row);
      }
    } else {
      if (use_sse2) {
        ConvolveVertically_SSE2<false>(filter_values, filter_length,
                                       first_row_for_filter,
                                       filter_x.num_values(), cur_output_row);
      } else {
        ConvolveVertically<false>(filter_values, filter_length,
                                 first_row_for_filter,
                                 filter_x.num_values(), cur_output_row);
      }
    }
  }
}

}  // namespace skia
