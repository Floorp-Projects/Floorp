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

#include "convolver.h"
#include <algorithm>
#include "skia/include/core/SkTypes.h"

#if defined(_MIPS_ARCH_LOONGSON3A)

#include "MMIHelpers.h"

namespace skia {

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the num_values() of the filter.
void ConvolveHorizontally_LS3(const unsigned char* src_data,
                               const ConvolutionFilter1D& filter,
                               unsigned char* out_row) {
  int num_values = filter.num_values();
  int tmp, filter_offset, filter_length;
  double zero, mask[4], shuf_50, shuf_fa;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "xor %[zero], %[zero], %[zero] \n\t"
    // |mask| will be used to decimate all extra filter coefficients that are
    // loaded by SIMD when |filter_length| is not divisible by 4.
    // mask[0] is not used in following algorithm.
    "li %[tmp], 1 \n\t"
    "dsll32 %[tmp], 0x10 \n\t"
    "daddiu %[tmp], -1 \n\t"
    "dmtc1 %[tmp], %[mask3] \n\t"
    "dsrl %[tmp], 0x10 \n\t"
    "mtc1 %[tmp], %[mask2] \n\t"
    "dsrl %[tmp], 0x10 \n\t"
    "mtc1 %[tmp], %[mask1] \n\t"
    "ori %[tmp], $0, 0x50 \n\t"
    "mtc1 %[tmp], %[shuf_50] \n\t"
    "ori %[tmp], $0, 0xfa \n\t"
    "mtc1 %[tmp], %[shuf_fa] \n\t"
    ".set pop \n\t"
    :[zero]"=f"(zero), [mask1]"=f"(mask[1]),
     [mask2]"=f"(mask[2]), [mask3]"=f"(mask[3]),
     [shuf_50]"=f"(shuf_50), [shuf_fa]"=f"(shuf_fa),
     [tmp]"=&r"(tmp)
  );

  // Output one pixel each iteration, calculating all channels (RGBA) together.
  for (int out_x = 0; out_x < num_values; out_x++) {
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);
    double accumh, accuml;
    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const void *row_to_filter =
        reinterpret_cast<const void*>(&src_data[filter_offset << 2]);

    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      _mm_xor(accum, accum, accum)
      ".set pop \n\t"
      :[accumh]"=f"(accumh), [accuml]"=f"(accuml)
    );

    // We will load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < filter_length >> 2; filter_x++) {
      double src16h, src16l, mul_hih, mul_hil, mul_loh, mul_lol;
      double coeffh, coeffl, src8h, src8l, th, tl, coeff16h, coeff16l;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        // Load 4 coefficients => duplicate 1st and 2nd of them for all channels.
        // [16] xx xx xx xx c3 c2 c1 c0
        "gsldlc1 %[coeffl], 7(%[fval]) \n\t"
        "gsldrc1 %[coeffl], (%[fval]) \n\t"
        "xor %[coeffh], %[coeffh], %[coeffh] \n\t"
        // [16] xx xx xx xx c1 c1 c0 c0
        _mm_pshuflh(coeff16, coeff, shuf_50)
        // [16] c1 c1 c1 c1 c0 c0 c0 c0
        _mm_punpcklhw(coeff16, coeff16, coeff16)
        // Load four pixels => unpack the first two pixels to 16 bits =>
        // multiply with coefficients => accumulate the convolution result.
        // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
        "gsldlc1 %[src8h], 0xf(%[rtf]) \n\t"
        "gsldrc1 %[src8h], 0x8(%[rtf]) \n\t"
        "gsldlc1 %[src8l], 0x7(%[rtf]) \n\t"
        "gsldrc1 %[src8l], 0x0(%[rtf]) \n\t"
        // [16] a1 b1 g1 r1 a0 b0 g0 r0
        _mm_punpcklbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        // [32]  a0*c0 b0*c0 g0*c0 r0*c0
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        // [32]  a1*c1 b1*c1 g1*c1 r1*c1
        _mm_punpckhhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        // Duplicate 3rd and 4th coefficients for all channels =>
        // unpack the 3rd and 4th pixels to 16 bits => multiply with coefficients
        // => accumulate the convolution results.
        // [16] xx xx xx xx c3 c3 c2 c2
        _mm_pshuflh(coeff16, coeff, shuf_fa)
        // [16] c3 c3 c3 c3 c2 c2 c2 c2
        _mm_punpcklhw(coeff16, coeff16, coeff16)
        // [16] a3 g3 b3 r3 a2 g2 b2 r2
        _mm_punpckhbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        // [32]  a2*c2 b2*c2 g2*c2 r2*c2
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        // [32]  a3*c3 b3*c3 g3*c3 r3*c3
        _mm_punpckhhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        ".set pop \n\t"
        :[th]"=&f"(th), [tl]"=&f"(tl),
         [src8h]"=&f"(src8h), [src8l]"=&f"(src8l),
         [accumh]"+f"(accumh), [accuml]"+f"(accuml),
         [src16h]"=&f"(src16h), [src16l]"=&f"(src16l),
         [coeffh]"=&f"(coeffh), [coeffl]"=&f"(coeffl),
         [coeff16h]"=&f"(coeff16h), [coeff16l]"=&f"(coeff16l),
         [mul_hih]"=&f"(mul_hih), [mul_hil]"=&f"(mul_hil),
         [mul_loh]"=&f"(mul_loh), [mul_lol]"=&f"(mul_lol)
        :[zeroh]"f"(zero), [zerol]"f"(zero),
         [shuf_50]"f"(shuf_50), [shuf_fa]"f"(shuf_fa),
         [fval]"r"(filter_values), [rtf]"r"(row_to_filter)
      );

      // Advance the pixel and coefficients pointers.
      row_to_filter += 16;
      filter_values += 4;
    }

    // When |filter_length| is not divisible by 4, we need to decimate some of
    // the filter coefficient that was loaded incorrectly to zero; Other than
    // that the algorithm is same with above, except that the 4th pixel will be
    // always absent.
    int r = filter_length & 3;
    if (r) {
      double coeffh, coeffl, th, tl, coeff16h, coeff16l;
      double src8h, src8l, src16h, src16l, mul_hih, mul_hil, mul_loh, mul_lol;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "gsldlc1 %[coeffl], 7(%[fval]) \n\t"
        "gsldrc1 %[coeffl], (%[fval]) \n\t"
        "xor %[coeffh], %[coeffh], %[coeffh] \n\t"
        // Mask out extra filter taps.
        "and %[coeffl], %[coeffl], %[mask] \n\t"
        _mm_pshuflh(coeff16, coeff, shuf_50)
        _mm_punpcklhw(coeff16, coeff16, coeff16)
        "gsldlc1 %[src8h], 0xf(%[rtf]) \n\t"
        "gsldrc1 %[src8h], 0x8(%[rtf]) \n\t"
        "gsldlc1 %[src8l], 0x7(%[rtf]) \n\t"
        "gsldrc1 %[src8l], 0x0(%[rtf]) \n\t"
        _mm_punpcklbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        _mm_punpckhhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        _mm_punpckhbh(src16, src8, zero)
        _mm_pshuflh(coeff16, coeff, shuf_fa)
        _mm_punpcklhw(coeff16, coeff16, coeff16)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum, accum, t)
        ".set pop \n\t"
        :[th]"=&f"(th), [tl]"=&f"(tl),
         [src8h]"=&f"(src8h), [src8l]"=&f"(src8l),
         [accumh]"+f"(accumh), [accuml]"+f"(accuml),
         [src16h]"=&f"(src16h), [src16l]"=&f"(src16l),
         [coeffh]"=&f"(coeffh), [coeffl]"=&f"(coeffl),
         [coeff16h]"=&f"(coeff16h), [coeff16l]"=&f"(coeff16l),
         [mul_hih]"=&f"(mul_hih), [mul_hil]"=&f"(mul_hil),
         [mul_loh]"=&f"(mul_loh), [mul_lol]"=&f"(mul_lol)
        :[fval]"r"(filter_values), [rtf]"r"(row_to_filter),
         [zeroh]"f"(zero), [zerol]"f"(zero), [mask]"f"(mask[r]),
         [shuf_50]"f"(shuf_50), [shuf_fa]"f"(shuf_fa)
      );
    }

    double t, sra;
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      "ori %[tmp], $0, %[sk_sra] \n\t"
      "mtc1 %[tmp], %[sra] \n\t"
      // Shift right for fixed point implementation.
      _mm_psraw(accum, accum, sra)
      // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
      _mm_packsswh(accum, accum, zero, t)
      // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
      _mm_packushb(accum, accum, zero, t)
      // Store the pixel value of 32 bits.
      "swc1 %[accuml], (%[out_row]) \n\t"
      ".set pop \n\t"
      :[sra]"=&f"(sra), [t]"=&f"(t), [tmp]"=&r"(tmp),
       [accumh]"+f"(accumh), [accuml]"+f"(accuml)
      :[sk_sra]"i"(ConvolutionFilter1D::kShiftBits),
       [out_row]"r"(out_row), [zeroh]"f"(zero), [zerol]"f"(zero)
      :"memory"
    );

    out_row += 4;
  }
}

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the [begin, end) of the filter.
// Process one pixel at a time.
void ConvolveHorizontally1_LS3(const unsigned char* src_data,
                               const ConvolutionFilter1D& filter,
                               unsigned char* out_row) {
  int num_values = filter.num_values();
  double zero;
  double sra;

  asm volatile (
    ".set push \n"
    ".set arch=loongson3a \n"
    "xor %[zero], %[zero], %[zero] \n"
    "mtc1 %[sk_sra], %[sra] \n"
    ".set pop \n"
    :[zero]"=&f"(zero), [sra]"=&f"(sra)
    :[sk_sra]"r"(ConvolutionFilter1D::kShiftBits)
  );
  // Loop over each pixel on this row in the output image.
  for (int out_x = 0; out_x < num_values; out_x++) {
    // Get the filter that determines the current output pixel.
    int filter_offset;
    int filter_length;
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const unsigned char* row_to_filter = &src_data[filter_offset * 4];

    // Apply the filter to the row to get the destination pixel in |accum|.
    double accuml;
    double accumh;
    asm volatile (
      ".set push \n"
      ".set arch=loongson3a \n"
      "xor %[accuml], %[accuml], %[accuml] \n"
      "xor %[accumh], %[accumh], %[accumh] \n"
      ".set pop \n"
      :[accuml]"=&f"(accuml), [accumh]"=&f"(accumh)
    );
    for (int filter_x = 0; filter_x < filter_length; filter_x++) {
      double src8;
      double src16;
      double coeff;
      double coeff16;
      asm volatile (
        ".set push \n"
        ".set arch=loongson3a \n"
        "lwc1 %[src8], %[rtf] \n"
        "mtc1 %[fv], %[coeff] \n"
        "pshufh %[coeff16], %[coeff], %[zero] \n"
        "punpcklbh %[src16], %[src8], %[zero] \n"
        "pmullh %[src8], %[src16], %[coeff16] \n"
        "pmulhh %[coeff], %[src16], %[coeff16] \n"
        "punpcklhw %[src16], %[src8], %[coeff] \n"
        "punpckhhw %[coeff16], %[src8], %[coeff] \n"
        "paddw %[accuml], %[accuml], %[src16] \n"
        "paddw %[accumh], %[accumh], %[coeff16] \n"
        ".set pop \n"
        :[accuml]"+f"(accuml), [accumh]"+f"(accumh),
         [src8]"=&f"(src8), [src16]"=&f"(src16),
         [coeff]"=&f"(coeff), [coeff16]"=&f"(coeff16)
        :[rtf]"m"(row_to_filter[filter_x * 4]),
         [fv]"r"(filter_values[filter_x]), [zero]"f"(zero)
      );
    }

    asm volatile (
      ".set push \n"
      ".set arch=loongson3a \n"
      // Bring this value back in range. All of the filter scaling factors
      // are in fixed point with kShiftBits bits of fractional part.
      "psraw %[accuml], %[accuml], %[sra] \n"
      "psraw %[accumh], %[accumh], %[sra] \n"
      // Store the new pixel.
      "packsswh %[accuml], %[accuml], %[accumh] \n"
      "packushb %[accuml], %[accuml], %[zero] \n"
      "swc1 %[accuml], %[out_row] \n"
      ".set pop \n"
      :[accuml]"+f"(accuml), [accumh]"+f"(accumh)
      :[sra]"f"(sra), [zero]"f"(zero), [out_row]"m"(out_row[out_x * 4])
      :"memory"
    );
  }
}

// Convolves horizontally along four rows. The row data is given in
// |src_data| and continues for the num_values() of the filter.
// The algorithm is almost same as |ConvolveHorizontally_LS3|. Please
// refer to that function for detailed comments.
void ConvolveHorizontally4_LS3(const unsigned char* src_data[4],
                                const ConvolutionFilter1D& filter,
                                unsigned char* out_row[4]) {
  int num_values = filter.num_values();
  int tmp, filter_offset, filter_length;
  double zero, mask[4], shuf_50, shuf_fa;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "xor %[zero], %[zero], %[zero] \n\t"
    // |mask| will be used to decimate all extra filter coefficients that are
    // loaded by SIMD when |filter_length| is not divisible by 4.
    // mask[0] is not used in following algorithm.
    "li %[tmp], 1 \n\t"
    "dsll32 %[tmp], 0x10 \n\t"
    "daddiu %[tmp], -1 \n\t"
    "dmtc1 %[tmp], %[mask3] \n\t"
    "dsrl %[tmp], 0x10 \n\t"
    "mtc1 %[tmp], %[mask2] \n\t"
    "dsrl %[tmp], 0x10 \n\t"
    "mtc1 %[tmp], %[mask1] \n\t"
    "ori %[tmp], $0, 0x50 \n\t"
    "mtc1 %[tmp], %[shuf_50] \n\t"
    "ori %[tmp], $0, 0xfa \n\t"
    "mtc1 %[tmp], %[shuf_fa] \n\t"
    ".set pop \n\t"
    :[zero]"=f"(zero), [mask1]"=f"(mask[1]),
     [mask2]"=f"(mask[2]), [mask3]"=f"(mask[3]),
     [shuf_50]"=f"(shuf_50), [shuf_fa]"=f"(shuf_fa),
     [tmp]"=&r"(tmp)
  );

  // Output one pixel each iteration, calculating all channels (RGBA) together.
  for (int out_x = 0; out_x < num_values; out_x++) {
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);
    double accum0h, accum0l, accum1h, accum1l;
    double accum2h, accum2l, accum3h, accum3l;

    // four pixels in a column per iteration.
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      _mm_xor(accum0, accum0, accum0)
      _mm_xor(accum1, accum1, accum1)
      _mm_xor(accum2, accum2, accum2)
      _mm_xor(accum3, accum3, accum3)
      ".set pop \n\t"
      :[accum0h]"=f"(accum0h), [accum0l]"=f"(accum0l),
       [accum1h]"=f"(accum1h), [accum1l]"=f"(accum1l),
       [accum2h]"=f"(accum2h), [accum2l]"=f"(accum2l),
       [accum3h]"=f"(accum3h), [accum3l]"=f"(accum3l)
    );

    int start = (filter_offset<<2);
    // We will load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < (filter_length >> 2); filter_x++) {
      double src8h, src8l, src16h, src16l;
      double mul_hih, mul_hil, mul_loh, mul_lol, th, tl;
      double coeffh, coeffl, coeff16loh, coeff16lol, coeff16hih, coeff16hil;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        // [16] xx xx xx xx c3 c2 c1 c0
        "gsldlc1 %[coeffl], 7(%[fval]) \n\t"
        "gsldrc1 %[coeffl], (%[fval]) \n\t"
        "xor %[coeffh], %[coeffh], %[coeffh] \n\t"
        // [16] xx xx xx xx c1 c1 c0 c0
        _mm_pshuflh(coeff16lo, coeff, shuf_50)
        // [16] c1 c1 c1 c1 c0 c0 c0 c0
        _mm_punpcklhw(coeff16lo, coeff16lo, coeff16lo)
        // [16] xx xx xx xx c3 c3 c2 c2
        _mm_pshuflh(coeff16hi, coeff, shuf_fa)
        // [16] c3 c3 c3 c3 c2 c2 c2 c2
        _mm_punpcklhw(coeff16hi, coeff16hi, coeff16hi)
        ".set pop \n\t"
        :[coeffh]"=&f"(coeffh), [coeffl]"=&f"(coeffl),
         [coeff16loh]"=&f"(coeff16loh), [coeff16lol]"=&f"(coeff16lol),
         [coeff16hih]"=&f"(coeff16hih), [coeff16hil]"=&f"(coeff16hil)
        :[fval]"r"(filter_values), [shuf_50]"f"(shuf_50), [shuf_fa]"f"(shuf_fa)
      );

#define ITERATION(_src, _accumh, _accuml)                              \
      asm volatile (                                                   \
        ".set push \n\t"                                               \
        ".set arch=loongson3a \n\t"                                    \
        "gsldlc1 %[src8h], 0xf(%[src]) \n\t"                           \
        "gsldrc1 %[src8h], 0x8(%[src]) \n\t"                           \
        "gsldlc1 %[src8l], 0x7(%[src]) \n\t"                           \
        "gsldrc1 %[src8l], 0x0(%[src]) \n\t"                           \
        _mm_punpcklbh(src16, src8, zero)                               \
        _mm_pmulhh(mul_hi, src16, coeff16lo)                           \
        _mm_pmullh(mul_lo, src16, coeff16lo)                           \
        _mm_punpcklhw(t, mul_lo, mul_hi)                               \
        _mm_paddw(accum, accum, t)                                     \
        _mm_punpckhhw(t, mul_lo, mul_hi)                               \
        _mm_paddw(accum, accum, t)                                     \
        _mm_punpckhbh(src16, src8, zero)                               \
        _mm_pmulhh(mul_hi, src16, coeff16hi)                           \
        _mm_pmullh(mul_lo, src16, coeff16hi)                           \
        _mm_punpcklhw(t, mul_lo, mul_hi)                               \
        _mm_paddw(accum, accum, t)                                     \
        _mm_punpckhhw(t, mul_lo, mul_hi)                               \
        _mm_paddw(accum, accum, t)                                     \
        ".set pop \n\t"                                                \
        :[th]"=&f"(th), [tl]"=&f"(tl),                                 \
         [src8h]"=&f"(src8h), [src8l]"=&f"(src8l),                     \
         [src16h]"=&f"(src16h), [src16l]"=&f"(src16l),                 \
         [mul_hih]"=&f"(mul_hih), [mul_hil]"=&f"(mul_hil),             \
         [mul_loh]"=&f"(mul_loh), [mul_lol]"=&f"(mul_lol),             \
         [accumh]"+f"(_accumh), [accuml]"+f"(_accuml)                  \
        :[zeroh]"f"(zero), [zerol]"f"(zero), [src]"r"(_src),           \
         [coeff16loh]"f"(coeff16loh), [coeff16lol]"f"(coeff16lol),     \
         [coeff16hih]"f"(coeff16hih), [coeff16hil]"f"(coeff16hil)      \
      );

      ITERATION(src_data[0] + start, accum0h, accum0l);
      ITERATION(src_data[1] + start, accum1h, accum1l);
      ITERATION(src_data[2] + start, accum2h, accum2l);
      ITERATION(src_data[3] + start, accum3h, accum3l);

      start += 16;
      filter_values += 4;
    }

    int r = filter_length & 3;
    if (r) {
      double src8h, src8l, src16h, src16l;
      double mul_hih, mul_hil, mul_loh, mul_lol, th, tl;
      double coeffh, coeffl, coeff16loh, coeff16lol, coeff16hih, coeff16hil;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "gsldlc1 %[coeffl], 7(%[fval]) \n\t"
        "gsldrc1 %[coeffl], (%[fval]) \n\t"
        "xor %[coeffh], %[coeffh], %[coeffh] \n\t"
        // Mask out extra filter taps.
        "and %[coeffl], %[coeffl], %[mask] \n\t"
        _mm_pshuflh(coeff16lo, coeff, shuf_50)
        /* c1 c1 c1 c1 c0 c0 c0 c0 */
        _mm_punpcklhw(coeff16lo, coeff16lo, coeff16lo)
        _mm_pshuflh(coeff16hi, coeff, shuf_fa)
        _mm_punpcklhw(coeff16hi, coeff16hi, coeff16hi)
        ".set pop \n\t"
        :[coeffh]"=&f"(coeffh), [coeffl]"=&f"(coeffl),
         [coeff16loh]"=&f"(coeff16loh), [coeff16lol]"=&f"(coeff16lol),
         [coeff16hih]"=&f"(coeff16hih), [coeff16hil]"=&f"(coeff16hil)
        :[fval]"r"(filter_values), [mask]"f"(mask[r]),
         [shuf_50]"f"(shuf_50), [shuf_fa]"f"(shuf_fa)
      );

      ITERATION(src_data[0] + start, accum0h, accum0l);
      ITERATION(src_data[1] + start, accum1h, accum1l);
      ITERATION(src_data[2] + start, accum2h, accum2l);
      ITERATION(src_data[3] + start, accum3h, accum3l);
    }

    double t, sra;
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      "ori %[tmp], $0, %[sk_sra] \n\t"
      "mtc1 %[tmp], %[sra] \n\t"
      _mm_psraw(accum0, accum0, sra)
      _mm_packsswh(accum0, accum0, zero, t)
      _mm_packushb(accum0, accum0, zero, t)
      _mm_psraw(accum1, accum1, sra)
      _mm_packsswh(accum1, accum1, zero, t)
      _mm_packushb(accum1, accum1, zero, t)
      _mm_psraw(accum2, accum2, sra)
      _mm_packsswh(accum2, accum2, zero, t)
      _mm_packushb(accum2, accum2, zero, t)
      _mm_psraw(accum3, accum3, sra)
      _mm_packsswh(accum3, accum3, zero, t)
      _mm_packushb(accum3, accum3, zero, t)
      "swc1 %[accum0l], (%[out_row0]) \n\t"
      "swc1 %[accum1l], (%[out_row1]) \n\t"
      "swc1 %[accum2l], (%[out_row2]) \n\t"
      "swc1 %[accum3l], (%[out_row3]) \n\t"
      ".set pop \n\t"
      :[accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
       [accum1h]"+f"(accum1h), [accum1l]"+f"(accum1l),
       [accum2h]"+f"(accum2h), [accum2l]"+f"(accum2l),
       [accum3h]"+f"(accum3h), [accum3l]"+f"(accum3l),
       [sra]"=&f"(sra), [t]"=&f"(t), [tmp]"=&r"(tmp)
      :[zeroh]"f"(zero), [zerol]"f"(zero),
       [out_row0]"r"(out_row[0]), [out_row1]"r"(out_row[1]),
       [out_row2]"r"(out_row[2]), [out_row3]"r"(out_row[3]),
       [sk_sra]"i"(ConvolutionFilter1D::kShiftBits)
      :"memory"
    );

    out_row[0] += 4;
    out_row[1] += 4;
    out_row[2] += 4;
    out_row[3] += 4;
  }
}

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |source_data_rows| array, with each row
// being |pixel_width| wide.
//
// The output must have room for |pixel_width * 4| bytes.
template<bool has_alpha>
void ConvolveVertically_LS3_impl(const ConvolutionFilter1D::Fixed* filter_values,
                                  int filter_length,
                                  unsigned char* const* source_data_rows,
                                  int pixel_width,
                                  unsigned char* out_row) {
  uint64_t tmp;
  int width = pixel_width & ~3;
  double zero, sra, coeff16h, coeff16l;
  double accum0h, accum0l, accum1h, accum1l;
  double accum2h, accum2l, accum3h, accum3l;
  const void *src;
  int out_x;

  asm volatile (
    ".set push \n\t"
    ".set arch=loongson3a \n\t"
    "xor %[zero], %[zero], %[zero] \n\t"
    "ori %[tmp], $0, %[sk_sra] \n\t"
    "mtc1 %[tmp], %[sra] \n\t"
    ".set pop \n\t"
    :[zero]"=f"(zero), [sra]"=f"(sra), [tmp]"=&r"(tmp)
    :[sk_sra]"i"(ConvolutionFilter1D::kShiftBits)
  );

  // Output four pixels per iteration (16 bytes).
  for (out_x = 0; out_x < width; out_x += 4) {
    // Accumulated result for each pixel. 32 bits per RGBA channel.
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      _mm_xor(accum0, accum0, accum0)
      _mm_xor(accum1, accum1, accum1)
      _mm_xor(accum2, accum2, accum2)
      _mm_xor(accum3, accum3, accum3)
      ".set pop \n\t"
      :[accum0h]"=f"(accum0h), [accum0l]"=f"(accum0l),
       [accum1h]"=f"(accum1h), [accum1l]"=f"(accum1l),
       [accum2h]"=f"(accum2h), [accum2l]"=f"(accum2l),
       [accum3h]"=f"(accum3h), [accum3l]"=f"(accum3l)
    );

    // Convolve with one filter coefficient per iteration.
    for (int filter_y = 0; filter_y < filter_length; filter_y++) {
      double src8h, src8l, src16h, src16l;
      double mul_hih, mul_hil, mul_loh, mul_lol, th, tl;

      src = reinterpret_cast<const void*>(
          &source_data_rows[filter_y][out_x << 2]);

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        // Duplicate the filter coefficient 8 times.
        // [16] cj cj cj cj cj cj cj cj
        "gsldlc1 %[coeff16l], 7+%[fval] \n\t"
        "gsldrc1 %[coeff16l], %[fval] \n\t"
        "pshufh %[coeff16l], %[coeff16l], %[zerol] \n\t"
        "mov.d %[coeff16h], %[coeff16l] \n\t"
        // Load four pixels (16 bytes) together.
        // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
        "gsldlc1 %[src8h], 0xf(%[src]) \n\t"
        "gsldrc1 %[src8h], 0x8(%[src]) \n\t"
        "gsldlc1 %[src8l], 0x7(%[src]) \n\t"
        "gsldrc1 %[src8l], 0x0(%[src]) \n\t"
        // Unpack 1st and 2nd pixels from 8 bits to 16 bits for each channels =>
        // multiply with current coefficient => accumulate the result.
        // [16] a1 b1 g1 r1 a0 b0 g0 r0
        _mm_punpcklbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        // [32] a0 b0 g0 r0
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum0, accum0, t)
        // [32] a1 b1 g1 r1
        _mm_punpckhhw(t, mul_lo, mul_hi)
        _mm_paddw(accum1, accum1, t)
        // Unpack 3rd and 4th pixels from 8 bits to 16 bits for each channels =>
        // multiply with current coefficient => accumulate the result.
        // [16] a3 b3 g3 r3 a2 b2 g2 r2
        _mm_punpckhbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        ".set pop \n\t"
        :[th]"=&f"(th), [tl]"=&f"(tl),
         [src8h]"=&f"(src8h), [src8l]"=&f"(src8l),
         [src16h]"=&f"(src16h), [src16l]"=&f"(src16l),
         [mul_hih]"=&f"(mul_hih), [mul_hil]"=&f"(mul_hil),
         [mul_loh]"=&f"(mul_loh), [mul_lol]"=&f"(mul_lol),
         [accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
         [accum1h]"+f"(accum1h), [accum1l]"+f"(accum1l),
         [coeff16h]"=&f"(coeff16h), [coeff16l]"=&f"(coeff16l)
        :[zeroh]"f"(zero), [zerol]"f"(zero),
         [fval]"m"(filter_values[filter_y]),
         [src]"r"(src)
      );

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        // [32] a2 b2 g2 r2
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum2, accum2, t)
        // [32] a3 b3 g3 r3
        _mm_punpckhhw(t, mul_lo, mul_hi)
        _mm_paddw(accum3, accum3, t)
        ".set pop \n\t"
        :[th]"=&f"(th), [tl]"=&f"(tl),
         [mul_hih]"+f"(mul_hih), [mul_hil]"+f"(mul_hil),
         [mul_loh]"+f"(mul_loh), [mul_lol]"+f"(mul_lol),
         [accum2h]"+f"(accum2h), [accum2l]"+f"(accum2l),
         [accum3h]"+f"(accum3h), [accum3l]"+f"(accum3l)
      );
    }

    double t;
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      // Shift right for fixed point implementation.
      _mm_psraw(accum0, accum0, sra)
      _mm_psraw(accum1, accum1, sra)
      _mm_psraw(accum2, accum2, sra)
      _mm_psraw(accum3, accum3, sra)
      // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      _mm_packsswh(accum0, accum0, accum1, t)
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      _mm_packsswh(accum2, accum2, accum3, t)
      // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      _mm_packushb(accum0, accum0, accum2, t)
      ".set pop \n\t"
      :[accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
       [accum1h]"+f"(accum1h), [accum1l]"+f"(accum1l),
       [accum2h]"+f"(accum2h), [accum2l]"+f"(accum2l),
       [accum3h]"+f"(accum3h), [accum3l]"+f"(accum3l),
       [t]"=&f"(t)
      :[sra]"f"(sra)
    );

    if (has_alpha) {
      double ah, al, bh, bl, srl8, srl16, sll24;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "li %[tmp], 8 \n\t"
        "mtc1 %[tmp], %[srl8] \n\t"
        "li %[tmp], 16 \n\t"
        "mtc1 %[tmp], %[srl16] \n\t"
        "li %[tmp], 24 \n\t"
        "mtc1 %[tmp], %[sll24] \n\t"
        // Compute the max(ri, gi, bi) for each pixel.
        // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
        _mm_psraw(a, accum0, srl8)
        // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
        _mm_pmaxub(b, a, accum0) // Max of r and g.
        // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
        _mm_psrlw(a, accum0, srl16)
        // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
        _mm_pmaxub(b, a, b) // Max of r and g and b.
        // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
        _mm_psllw(b, b, sll24)
        // Make sure the value of alpha channel is always larger than maximum
        // value of color channels.
        _mm_pmaxub(accum0, b, accum0)
        ".set pop \n\t"
        :[accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
         [tmp]"=&r"(tmp), [ah]"=&f"(ah), [al]"=&f"(al),
         [bh]"=&f"(bh), [bl]"=&f"(bl), [srl8]"=&f"(srl8),
         [srl16]"=&f"(srl16), [sll24]"=&f"(sll24)
      );
    } else {
      double maskh, maskl;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        // Set value of alpha channels to 0xFF.
        "li %[tmp], 0xff000000 \n\t"
        "mtc1 %[tmp], %[maskl] \n\t"
        "punpcklwd %[maskl], %[maskl], %[maskl] \n\t"
        "mov.d %[maskh], %[maskl] \n\t"
        _mm_or(accum0, accum0, mask)
      ".set pop \n\t"
      :[maskh]"=&f"(maskh), [maskl]"=&f"(maskl),
       [accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
       [tmp]"=&r"(tmp)
      );
    }

    // Store the convolution result (16 bytes) and advance the pixel pointers.
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      "gssdlc1 %[accum0h], 0xf(%[out_row]) \n\t"
      "gssdrc1 %[accum0h], 0x8(%[out_row]) \n\t"
      "gssdlc1 %[accum0l], 0x7(%[out_row]) \n\t"
      "gssdrc1 %[accum0l], 0x0(%[out_row]) \n\t"
      ".set pop \n\t"
      ::[accum0h]"f"(accum0h), [accum0l]"f"(accum0l),
        [out_row]"r"(out_row)
      :"memory"
    );
    out_row += 16;
  }

  // When the width of the output is not divisible by 4, We need to save one
  // pixel (4 bytes) each time. And also the fourth pixel is always absent.
  if (pixel_width & 3) {
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      _mm_xor(accum0, accum0, accum0)
      _mm_xor(accum1, accum1, accum1)
      _mm_xor(accum2, accum2, accum2)
      ".set pop \n\t"
      :[accum0h]"=&f"(accum0h), [accum0l]"=&f"(accum0l),
       [accum1h]"=&f"(accum1h), [accum1l]"=&f"(accum1l),
       [accum2h]"=&f"(accum2h), [accum2l]"=&f"(accum2l)
    );
    for (int filter_y = 0; filter_y < filter_length; ++filter_y) {
      double src8h, src8l, src16h, src16l;
      double th, tl, mul_hih, mul_hil, mul_loh, mul_lol;
      src = reinterpret_cast<const void*>(
          &source_data_rows[filter_y][out_x<<2]);

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "gsldlc1 %[coeff16l], 7+%[fval] \n\t"
        "gsldrc1 %[coeff16l], %[fval] \n\t"
        "pshufh %[coeff16l], %[coeff16l], %[zerol] \n\t"
        "mov.d %[coeff16h], %[coeff16l] \n\t"
        // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
        "gsldlc1 %[src8h], 0xf(%[src]) \n\t"
        "gsldrc1 %[src8h], 0x8(%[src]) \n\t"
        "gsldlc1 %[src8l], 0x7(%[src]) \n\t"
        "gsldrc1 %[src8l], 0x0(%[src]) \n\t"
        // [16] a1 b1 g1 r1 a0 b0 g0 r0
        _mm_punpcklbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        // [32] a0 b0 g0 r0
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum0, accum0, t)
        // [32] a1 b1 g1 r1
        _mm_punpckhhw(t, mul_lo, mul_hi)
        _mm_paddw(accum1, accum1, t)
        // [16] a3 b3 g3 r3 a2 b2 g2 r2
        _mm_punpckhbh(src16, src8, zero)
        _mm_pmulhh(mul_hi, src16, coeff16)
        _mm_pmullh(mul_lo, src16, coeff16)
        // [32] a2 b2 g2 r2
        _mm_punpcklhw(t, mul_lo, mul_hi)
        _mm_paddw(accum2, accum2, t)
        ".set pop \n\t"
        :[th]"=&f"(th), [tl]"=&f"(tl),
         [src8h]"=&f"(src8h), [src8l]"=&f"(src8l),
         [src16h]"=&f"(src16h), [src16l]"=&f"(src16l),
         [mul_hih]"=&f"(mul_hih), [mul_hil]"=&f"(mul_hil),
         [mul_loh]"=&f"(mul_loh), [mul_lol]"=&f"(mul_lol),
         [accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
         [accum1h]"+f"(accum1h), [accum1l]"+f"(accum1l),
         [accum2h]"+f"(accum2h), [accum2l]"+f"(accum2l),
         [coeff16h]"=&f"(coeff16h), [coeff16l]"=&f"(coeff16l)
        :[zeroh]"f"(zero), [zerol]"f"(zero),
         [fval]"m"(filter_values[filter_y]),
         [src]"r"(src)
      );
    }

    double t;
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      _mm_psraw(accum0, accum0, sra)
      _mm_psraw(accum1, accum1, sra)
      _mm_psraw(accum2, accum2, sra)
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      _mm_packsswh(accum0, accum0, accum1, t)
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      _mm_packsswh(accum2, accum2, zero, t)
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      _mm_packushb(accum0, accum0, accum2, t)
      ".set pop \n\t"
      :[accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
       [accum1h]"+f"(accum1h), [accum1l]"+f"(accum1l),
       [accum2h]"+f"(accum2h), [accum2l]"+f"(accum2l),
       [t]"=&f"(t)
      :[zeroh]"f"(zero), [zerol]"f"(zero), [sra]"f"(sra)
    );
    if (has_alpha) {
      double ah, al, bh, bl, srl8, srl16, sll24;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "li %[tmp], 8 \n\t"
        "mtc1 %[tmp], %[srl8] \n\t"
        "li %[tmp], 16 \n\t"
        "mtc1 %[tmp], %[srl16] \n\t"
        "li %[tmp], 24 \n\t"
        "mtc1 %[tmp], %[sll24] \n\t"
        // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
        _mm_psrlw(a, accum0, srl8)
        // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
        _mm_pmaxub(b, a, accum0) // Max of r and g.
        // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
        _mm_psrlw(a, accum0, srl16)
        // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
        _mm_pmaxub(b, a, b) // Max of r and g and b.
        // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
        _mm_psllw(b, b, sll24)
        _mm_pmaxub(accum0, b, accum0)
        ".set pop \n\t"
        :[ah]"=&f"(ah), [al]"=&f"(al), [bh]"=&f"(bh), [bl]"=&f"(bl),
         [accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l), [tmp]"=&r"(tmp),
         [srl8]"=&f"(srl8), [srl16]"=&f"(srl16), [sll24]"=&f"(sll24)
      );
    } else {
      double maskh, maskl;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        // Set value of alpha channels to 0xFF.
        "li %[tmp], 0xff000000 \n\t"
        "mtc1 %[tmp], %[maskl] \n\t"
        "punpcklwd %[maskl], %[maskl], %[maskl] \n\t"
        "mov.d %[maskh], %[maskl] \n\t"
        _mm_or(accum0, accum0, mask)
        ".set pop \n\t"
        :[maskh]"=&f"(maskh), [maskl]"=&f"(maskl),
         [accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l),
         [tmp]"=&r"(tmp)
      );
    }

    double s4, s64;
    asm volatile (
      ".set push \n\t"
      ".set arch=loongson3a \n\t"
      "li %[tmp], 4 \n\t"
      "mtc1 %[tmp], %[s4] \n\t"
      "li %[tmp], 64 \n\t"
      "mtc1 %[tmp], %[s64] \n\t"
      ".set pop \n\t"
      :[s4]"=f"(s4), [s64]"=f"(s64),
       [tmp]"=&r"(tmp)
    );
    for (int out_x = width; out_x < pixel_width; out_x++) {
      double t;

      asm volatile (
        ".set push \n\t"
        ".set arch=loongson3a \n\t"
        "swc1 %[accum0l], (%[out_row]) \n\t"
        _mm_psrlq(accum0, accum0, s4, s64, t)
        ".set pop \n\t"
        :[t]"=&f"(t),
         [accum0h]"+f"(accum0h), [accum0l]"+f"(accum0l)
        :[out_row]"r"(out_row), [s4]"f"(s4), [s64]"f"(s64)
        :"memory"
      );
      out_row += 4;
    }
  }
}

void ConvolveVertically_LS3(const ConvolutionFilter1D::Fixed* filter_values,
                             int filter_length,
                             unsigned char* const* source_data_rows,
                             int pixel_width,
                             unsigned char* out_row, bool has_alpha) {
  if (has_alpha) {
    ConvolveVertically_LS3_impl<true>(filter_values, filter_length,
                                       source_data_rows, pixel_width, out_row);
  } else {
    ConvolveVertically_LS3_impl<false>(filter_values, filter_length,
                                       source_data_rows, pixel_width, out_row);
  }
}

}  // namespace skia

#endif /* _MIPS_ARCH_LOONGSON3A */
