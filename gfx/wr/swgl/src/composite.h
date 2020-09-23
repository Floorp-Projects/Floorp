/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

template <typename P>
static inline void scale_row(P* dst, int dstWidth, const P* src, int srcWidth,
                             int span) {
  int frac = 0;
  for (P* end = dst + span; dst < end; dst++) {
    *dst = *src;
    // Step source according to width ratio.
    for (frac += srcWidth; frac >= dstWidth; frac -= dstWidth) {
      src++;
    }
  }
}

static void scale_blit(Texture& srctex, const IntRect& srcReq, int srcZ,
                       Texture& dsttex, const IntRect& dstReq, int dstZ,
                       bool invertY, int bandOffset, int bandHeight) {
  // Cache scaling ratios
  int srcWidth = srcReq.width();
  int srcHeight = srcReq.height();
  int dstWidth = dstReq.width();
  int dstHeight = dstReq.height();
  // Compute valid dest bounds
  IntRect dstBounds = dsttex.sample_bounds(dstReq, invertY);
  // Compute valid source bounds
  // Scale source to dest, rounding inward to avoid sampling outside source
  IntRect srcBounds = srctex.sample_bounds(srcReq).scale(
      srcWidth, srcHeight, dstWidth, dstHeight, true);
  // Limit dest sampling bounds to overlap source bounds
  dstBounds.intersect(srcBounds);
  // Check if sampling bounds are empty
  if (dstBounds.is_empty()) {
    return;
  }
  // Compute final source bounds from clamped dest sampling bounds
  srcBounds =
      IntRect(dstBounds).scale(dstWidth, dstHeight, srcWidth, srcHeight);
  // Calculate source and dest pointers from clamped offsets
  int bpp = srctex.bpp();
  int srcStride = srctex.stride();
  int destStride = dsttex.stride();
  char* dest = dsttex.sample_ptr(dstReq, dstBounds, dstZ, invertY);
  char* src = srctex.sample_ptr(srcReq, srcBounds, srcZ);
  // Inverted Y must step downward along dest rows
  if (invertY) {
    destStride = -destStride;
  }
  int span = dstBounds.width();
  int frac = srcHeight * bandOffset;
  dest += destStride * bandOffset;
  src += srcStride * (frac / dstHeight);
  frac %= dstHeight;
  for (int rows = min(dstBounds.height() - bandOffset, bandHeight); rows > 0;
       rows--) {
    if (srcWidth == dstWidth) {
      // No scaling, so just do a fast copy.
      memcpy(dest, src, span * bpp);
    } else {
      // Do scaling with different source and dest widths.
      switch (bpp) {
        case 1:
          scale_row((uint8_t*)dest, dstWidth, (uint8_t*)src, srcWidth, span);
          break;
        case 2:
          scale_row((uint16_t*)dest, dstWidth, (uint16_t*)src, srcWidth, span);
          break;
        case 4:
          scale_row((uint32_t*)dest, dstWidth, (uint32_t*)src, srcWidth, span);
          break;
        default:
          assert(false);
          break;
      }
    }
    dest += destStride;
    // Step source according to height ratio.
    for (frac += srcHeight; frac >= dstHeight; frac -= dstHeight) {
      src += srcStride;
    }
  }
}

static void linear_row_blit(uint32_t* dest, int span, const vec2_scalar& srcUV,
                            float srcDU, int srcZOffset,
                            sampler2DArray sampler) {
  vec2 uv = init_interp(srcUV, vec2_scalar(srcDU, 0.0f));
  for (; span >= 4; span -= 4) {
    auto srcpx = textureLinearPackedRGBA8(sampler, ivec2(uv), srcZOffset);
    unaligned_store(dest, srcpx);
    dest += 4;
    uv.x += 4 * srcDU;
  }
  if (span > 0) {
    auto srcpx = textureLinearPackedRGBA8(sampler, ivec2(uv), srcZOffset);
    auto mask = span_mask_RGBA8(span);
    auto dstpx = unaligned_load<PackedRGBA8>(dest);
    unaligned_store(dest, (mask & dstpx) | (~mask & srcpx));
  }
}

static void linear_row_blit(uint8_t* dest, int span, const vec2_scalar& srcUV,
                            float srcDU, int srcZOffset,
                            sampler2DArray sampler) {
  vec2 uv = init_interp(srcUV, vec2_scalar(srcDU, 0.0f));
  for (; span >= 4; span -= 4) {
    auto srcpx = textureLinearPackedR8(sampler, ivec2(uv), srcZOffset);
    unaligned_store(dest, pack(srcpx));
    dest += 4;
    uv.x += 4 * srcDU;
  }
  if (span > 0) {
    auto srcpx = textureLinearPackedR8(sampler, ivec2(uv), srcZOffset);
    auto mask = span_mask_R8(span);
    auto dstpx = unpack(unaligned_load<PackedR8>(dest));
    unaligned_store(dest, pack((mask & dstpx) | (~mask & srcpx)));
  }
}

static void linear_row_blit(uint16_t* dest, int span, const vec2_scalar& srcUV,
                            float srcDU, int srcZOffset,
                            sampler2DArray sampler) {
  vec2 uv = init_interp(srcUV, vec2_scalar(srcDU, 0.0f));
  for (; span >= 4; span -= 4) {
    auto srcpx = textureLinearPackedRG8(sampler, ivec2(uv), srcZOffset);
    unaligned_store(dest, srcpx);
    dest += 4;
    uv.x += 4 * srcDU;
  }
  if (span > 0) {
    auto srcpx = textureLinearPackedRG8(sampler, ivec2(uv), srcZOffset);
    auto mask = span_mask_RG8(span);
    auto dstpx = unaligned_load<PackedRG8>(dest);
    unaligned_store(dest, (mask & dstpx) | (~mask & srcpx));
  }
}

static void linear_blit(Texture& srctex, const IntRect& srcReq, int srcZ,
                        Texture& dsttex, const IntRect& dstReq, int dstZ,
                        bool invertY, int bandOffset, int bandHeight) {
  assert(srctex.internal_format == GL_RGBA8 ||
         srctex.internal_format == GL_R8 || srctex.internal_format == GL_RG8);
  // Compute valid dest bounds
  IntRect dstBounds = dsttex.sample_bounds(dstReq, invertY);
  // Check if sampling bounds are empty
  if (dstBounds.is_empty()) {
    return;
  }
  // Initialize sampler for source texture
  sampler2DArray_impl sampler;
  init_sampler(&sampler, srctex);
  init_depth(&sampler, srctex);
  sampler.filter = TextureFilter::LINEAR;
  // Compute source UVs
  int srcZOffset = srcZ * sampler.height_stride;
  vec2_scalar srcUV(srcReq.x0, srcReq.y0);
  vec2_scalar srcDUV(float(srcReq.width()) / dstReq.width(),
                     float(srcReq.height()) / dstReq.height());
  // Skip to clamped source start
  srcUV += srcDUV * (vec2_scalar(dstBounds.x0, dstBounds.y0) + 0.5f);
  // Scale UVs by lerp precision
  srcUV = linearQuantize(srcUV, 128);
  srcDUV *= 128.0f;
  // Calculate dest pointer from clamped offsets
  int bpp = dsttex.bpp();
  int destStride = dsttex.stride();
  char* dest = dsttex.sample_ptr(dstReq, dstBounds, dstZ, invertY);
  // Inverted Y must step downward along dest rows
  if (invertY) {
    destStride = -destStride;
  }
  dest += destStride * bandOffset;
  srcUV.y += srcDUV.y * bandOffset;
  int span = dstBounds.width();
  for (int rows = min(dstBounds.height() - bandOffset, bandHeight); rows > 0;
       rows--) {
    switch (bpp) {
      case 1:
        linear_row_blit((uint8_t*)dest, span, srcUV, srcDUV.x, srcZOffset,
                        &sampler);
        break;
      case 2:
        linear_row_blit((uint16_t*)dest, span, srcUV, srcDUV.x, srcZOffset,
                        &sampler);
        break;
      case 4:
        linear_row_blit((uint32_t*)dest, span, srcUV, srcDUV.x, srcZOffset,
                        &sampler);
        break;
      default:
        assert(false);
        break;
    }
    dest += destStride;
    srcUV.y += srcDUV.y;
  }
}

static void linear_row_composite(uint32_t* dest, int span,
                                 const vec2_scalar& srcUV, float srcDU,
                                 sampler2D sampler) {
  vec2 uv = init_interp(srcUV, vec2_scalar(srcDU, 0.0f));
  for (; span >= 4; span -= 4) {
    WideRGBA8 srcpx = textureLinearUnpackedRGBA8(sampler, ivec2(uv), 0);
    WideRGBA8 dstpx = unpack(unaligned_load<PackedRGBA8>(dest));
    PackedRGBA8 r = pack(srcpx + dstpx - muldiv255(dstpx, alphas(srcpx)));
    unaligned_store(dest, r);

    dest += 4;
    uv.x += 4 * srcDU;
  }
  if (span > 0) {
    WideRGBA8 srcpx = textureLinearUnpackedRGBA8(sampler, ivec2(uv), 0);
    PackedRGBA8 dstpx = unaligned_load<PackedRGBA8>(dest);
    WideRGBA8 dstpxu = unpack(dstpx);
    PackedRGBA8 r = pack(srcpx + dstpxu - muldiv255(dstpxu, alphas(srcpx)));

    auto mask = span_mask_RGBA8(span);
    unaligned_store(dest, (mask & dstpx) | (~mask & r));
  }
}

static void linear_composite(Texture& srctex, const IntRect& srcReq,
                             Texture& dsttex, const IntRect& dstReq,
                             bool invertY, int bandOffset, int bandHeight) {
  assert(srctex.bpp() == 4);
  assert(dsttex.bpp() == 4);
  // Compute valid dest bounds
  IntRect dstBounds = dsttex.sample_bounds(dstReq, invertY);
  // Check if sampling bounds are empty
  if (dstBounds.is_empty()) {
    return;
  }
  // Initialize sampler for source texture
  sampler2D_impl sampler;
  init_sampler(&sampler, srctex);
  sampler.filter = TextureFilter::LINEAR;
  // Compute source UVs
  vec2_scalar srcUV(srcReq.x0, srcReq.y0);
  vec2_scalar srcDUV(float(srcReq.width()) / dstReq.width(),
                     float(srcReq.height()) / dstReq.height());
  // Skip to clamped source start
  srcUV += srcDUV * (vec2_scalar(dstBounds.x0, dstBounds.y0) + 0.5f);
  // Scale UVs by lerp precision
  srcUV = linearQuantize(srcUV, 128);
  srcDUV *= 128.0f;
  // Calculate dest pointer from clamped offsets
  int destStride = dsttex.stride();
  char* dest = dsttex.sample_ptr(dstReq, dstBounds, 0, invertY);
  // Inverted Y must step downward along dest rows
  if (invertY) {
    destStride = -destStride;
  }
  dest += destStride * bandOffset;
  srcUV.y += srcDUV.y * bandOffset;
  int span = dstBounds.width();
  for (int rows = min(dstBounds.height(), bandHeight); rows > 0; rows--) {
    linear_row_composite((uint32_t*)dest, span, srcUV, srcDUV.x, &sampler);
    dest += destStride;
    srcUV.y += srcDUV.y;
  }
}

extern "C" {

void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                     GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                     GLbitfield mask, GLenum filter) {
  assert(mask == GL_COLOR_BUFFER_BIT);
  Framebuffer* srcfb = get_framebuffer(GL_READ_FRAMEBUFFER);
  if (!srcfb || srcfb->layer < 0) return;
  Framebuffer* dstfb = get_framebuffer(GL_DRAW_FRAMEBUFFER);
  if (!dstfb || dstfb->layer < 0) return;
  Texture& srctex = ctx->textures[srcfb->color_attachment];
  if (!srctex.buf || srcfb->layer >= max(srctex.depth, 1)) return;
  Texture& dsttex = ctx->textures[dstfb->color_attachment];
  if (!dsttex.buf || dstfb->layer >= max(dsttex.depth, 1)) return;
  assert(!dsttex.locked);
  if (srctex.internal_format != dsttex.internal_format) {
    assert(false);
    return;
  }
  // Force flipped Y onto dest coordinates
  if (srcY1 < srcY0) {
    swap(srcY0, srcY1);
    swap(dstY0, dstY1);
  }
  bool invertY = dstY1 < dstY0;
  if (invertY) {
    swap(dstY0, dstY1);
  }
  IntRect srcReq = {srcX0, srcY0, srcX1, srcY1};
  IntRect dstReq = {dstX0, dstY0, dstX1, dstY1};
  if (srcReq.is_empty() || dstReq.is_empty()) {
    return;
  }
  prepare_texture(srctex);
  prepare_texture(dsttex, &dstReq);
  if (!srcReq.same_size(dstReq) && filter == GL_LINEAR &&
      (srctex.internal_format == GL_RGBA8 || srctex.internal_format == GL_R8 ||
       srctex.internal_format == GL_RG8)) {
    linear_blit(srctex, srcReq, srcfb->layer, dsttex, dstReq, dstfb->layer,
                invertY, 0, dstReq.height());
  } else {
    scale_blit(srctex, srcReq, srcfb->layer, dsttex, dstReq, dstfb->layer,
               invertY, 0, dstReq.height());
  }
}

typedef Texture LockedTexture;

// Lock the given texture to prevent modification.
LockedTexture* LockTexture(GLuint texId) {
  Texture& tex = ctx->textures[texId];
  if (!tex.buf) {
    return nullptr;
  }
  if (__sync_fetch_and_add(&tex.locked, 1) == 0) {
    // If this is the first time locking the texture, flush any delayed clears.
    prepare_texture(tex);
  }
  return (LockedTexture*)&tex;
}

// Lock the given framebuffer's color attachment to prevent modification.
LockedTexture* LockFramebuffer(GLuint fboId) {
  Framebuffer& fb = ctx->framebuffers[fboId];
  // Only allow locking a framebuffer if it has a valid color attachment and
  // only if targeting the first layer.
  if (!fb.color_attachment || fb.layer > 0) {
    return nullptr;
  }
  return LockTexture(fb.color_attachment);
}

// Reference an already locked resource
void LockResource(LockedTexture* resource) {
  if (!resource) {
    return;
  }
  __sync_fetch_and_add(&resource->locked, 1);
}

// Remove a lock on a texture that has been previously locked
void UnlockResource(LockedTexture* resource) {
  if (!resource) {
    return;
  }
  if (__sync_fetch_and_add(&resource->locked, -1) <= 0) {
    // The lock should always be non-zero before unlocking.
    assert(0);
  }
}

// Get the underlying buffer for a locked resource
void* GetResourceBuffer(LockedTexture* resource, int32_t* width,
                        int32_t* height, int32_t* stride) {
  *width = resource->width;
  *height = resource->height;
  *stride = resource->stride();
  return resource->buf;
}

// Extension for optimized compositing of textures or framebuffers that may be
// safely used across threads. The source and destination must be locked to
// ensure that they can be safely accessed while the SWGL context might be used
// by another thread. Band extents along the Y axis may be used to clip the
// destination rectangle without effecting the integer scaling ratios.
void Composite(LockedTexture* lockedDst, LockedTexture* lockedSrc, GLint srcX,
               GLint srcY, GLsizei srcWidth, GLsizei srcHeight, GLint dstX,
               GLint dstY, GLsizei dstWidth, GLsizei dstHeight,
               GLboolean opaque, GLboolean flip, GLenum filter,
               GLint bandOffset, GLsizei bandHeight) {
  if (!lockedDst || !lockedSrc) {
    return;
  }
  Texture& srctex = *lockedSrc;
  Texture& dsttex = *lockedDst;
  assert(srctex.bpp() == 4);
  assert(dsttex.bpp() == 4);

  IntRect srcReq = {srcX, srcY, srcX + srcWidth, srcY + srcHeight};
  IntRect dstReq = {dstX, dstY, dstX + dstWidth, dstY + dstHeight};

  if (opaque) {
    if (!srcReq.same_size(dstReq) && filter == GL_LINEAR) {
      linear_blit(srctex, srcReq, 0, dsttex, dstReq, 0, flip, bandOffset,
                  bandHeight);
    } else {
      scale_blit(srctex, srcReq, 0, dsttex, dstReq, 0, flip, bandOffset,
                 bandHeight);
    }
  } else {
    if (!srcReq.same_size(dstReq) || filter == GL_LINEAR) {
      linear_composite(srctex, srcReq, dsttex, dstReq, flip, bandOffset,
                       bandHeight);
    } else {
      const int bpp = 4;
      IntRect bounds = dsttex.sample_bounds(dstReq, flip);
      bounds.intersect(srctex.sample_bounds(srcReq));
      char* dest = dsttex.sample_ptr(dstReq, bounds, 0, flip);
      char* src = srctex.sample_ptr(srcReq, bounds, 0);
      int srcStride = srctex.stride();
      int destStride = dsttex.stride();
      if (flip) {
        destStride = -destStride;
      }
      dest += destStride * bandOffset;
      src += srcStride * bandOffset;
      for (int rows = min(bounds.height() - bandOffset, bandHeight); rows > 0;
           rows--) {
        char* end = src + bounds.width() * bpp;
        while (src + 4 * bpp <= end) {
          WideRGBA8 srcpx = unpack(unaligned_load<PackedRGBA8>(src));
          WideRGBA8 dstpx = unpack(unaligned_load<PackedRGBA8>(dest));
          PackedRGBA8 r = pack(srcpx + dstpx - muldiv255(dstpx, alphas(srcpx)));
          unaligned_store(dest, r);
          src += 4 * bpp;
          dest += 4 * bpp;
        }
        if (src < end) {
          WideRGBA8 srcpx = unpack(unaligned_load<PackedRGBA8>(src));
          WideRGBA8 dstpx = unpack(unaligned_load<PackedRGBA8>(dest));
          U32 r = bit_cast<U32>(
              pack(srcpx + dstpx - muldiv255(dstpx, alphas(srcpx))));
          unaligned_store(dest, r.x);
          if (src + bpp < end) {
            unaligned_store(dest + bpp, r.y);
            if (src + 2 * bpp < end) {
              unaligned_store(dest + 2 * bpp, r.z);
            }
          }
          dest += end - src;
          src = end;
        }
        dest += destStride - bounds.width() * bpp;
        src += srcStride - bounds.width() * bpp;
      }
    }
  }
}

}  // extern "C"

// Saturated add helper for YUV conversion. Supported platforms have intrinsics
// to do this natively, but support a slower generic fallback just in case.
static inline V8<int16_t> addsat(V8<int16_t> x, V8<int16_t> y) {
#if USE_SSE2
  return _mm_adds_epi16(x, y);
#elif USE_NEON
  return vqaddq_s16(x, y);
#else
  auto r = x + y;
  // An overflow occurred if the signs of both inputs x and y did not differ
  // but yet the sign of the result did differ.
  auto overflow = (~(x ^ y) & (r ^ x)) >> 15;
  // If there was an overflow, we need to choose the appropriate limit to clamp
  // to depending on whether or not the inputs are negative.
  auto limit = (x >> 15) ^ 0x7FFF;
  // If we didn't overflow, just use the result, and otherwise, use the limit.
  return (~overflow & r) | (overflow & limit);
#endif
}

// Interleave and packing helper for YUV conversion. During transform by the
// color matrix, the color components are de-interleaved as this format is
// usually what comes out of the planar YUV textures. The components thus need
// to be interleaved before finally getting packed to BGRA format. Alpha is
// forced to be opaque.
static inline PackedRGBA8 packYUV(V8<int16_t> gg, V8<int16_t> br) {
  return pack(bit_cast<WideRGBA8>(zip(br, gg))) |
         PackedRGBA8{0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255};
}

enum YUVColorSpace { REC_601 = 0, REC_709, REC_2020, IDENTITY };

// clang-format off
// Supports YUV color matrixes of the form:
// [R]   [1.1643835616438356,  0.0,  rv ]   [Y -  16]
// [G] = [1.1643835616438358, -gu,  -gv ] x [U - 128]
// [B]   [1.1643835616438356,  bu,  0.0 ]   [V - 128]
// We must be able to multiply a YUV input by a matrix coefficient ranging as
// high as ~2.2 in the U/V cases, where U/V can be signed values between -128
// and 127. The largest fixed-point representation we can thus support without
// overflowing 16 bit integers leaves us 6 bits of fractional precision while
// also supporting a sign bit. The closest representation of the Y coefficient
// ~1.164 in this precision is 74.5/2^6 which is common to all color spaces
// we support. Conversions can still sometimes overflow the precision and
// require clamping back into range, so we use saturated additions to do this
// efficiently at no extra cost.
// clang-format on
template <const double MATRIX[4]>
struct YUVConverterImpl {
  static inline PackedRGBA8 convert(V8<int16_t> yy, V8<int16_t> uv) {
    // Convert matrix coefficients to fixed-point representation.
    constexpr int16_t mrv = int16_t(MATRIX[0] * 64.0 + 0.5);
    constexpr int16_t mgu = -int16_t(MATRIX[1] * -64.0 + 0.5);
    constexpr int16_t mgv = -int16_t(MATRIX[2] * -64.0 + 0.5);
    constexpr int16_t mbu = int16_t(MATRIX[3] * 64.0 + 0.5);

    // Bias Y values by -16 and multiply by 74.5. Add 2^5 offset to round to
    // nearest 2^6.
    yy = yy * 74 + (yy >> 1) + (int16_t(-16 * 74.5) + (1 << 5));

    // Bias U/V values by -128.
    uv -= 128;

    // Compute (R, B) = (74.5*Y + rv*V, 74.5*Y + bu*U)
    auto br = V8<int16_t>{mbu, mrv, mbu, mrv, mbu, mrv, mbu, mrv} * uv;
    br = addsat(yy, br);
    br >>= 6;

    // Compute G = 74.5*Y + -gu*U + -gv*V
    auto gg = V8<int16_t>{mgu, mgv, mgu, mgv, mgu, mgv, mgu, mgv} * uv;
    gg = addsat(
        yy,
        addsat(gg, bit_cast<V8<int16_t>>(bit_cast<V4<uint32_t>>(gg) >> 16)));
    gg >>= 6;

    // Interleave B/R and G values. Force alpha to opaque.
    return packYUV(gg, br);
  }
};

template <YUVColorSpace COLOR_SPACE>
struct YUVConverter {};

// clang-format off
// From Rec601:
// [R]   [1.1643835616438356,  0.0,                 1.5960267857142858   ]   [Y -  16]
// [G] = [1.1643835616438358, -0.3917622900949137, -0.8129676472377708   ] x [U - 128]
// [B]   [1.1643835616438356,  2.017232142857143,   8.862867620416422e-17]   [V - 128]
// clang-format on
static constexpr double YUVMatrix601[4] = {
    1.5960267857142858, -0.3917622900949137, -0.8129676472377708,
    2.017232142857143};
template <>
struct YUVConverter<REC_601> : YUVConverterImpl<YUVMatrix601> {};

// clang-format off
// From Rec709:
// [R]   [1.1643835616438356,  0.0,                    1.7927410714285714]   [Y -  16]
// [G] = [1.1643835616438358, -0.21324861427372963,   -0.532909328559444 ] x [U - 128]
// [B]   [1.1643835616438356,  2.1124017857142854,     0.0               ]   [V - 128]
// clang-format on
static constexpr double YUVMatrix709[4] = {
    1.7927410714285714, -0.21324861427372963, -0.532909328559444,
    2.1124017857142854};
template <>
struct YUVConverter<REC_709> : YUVConverterImpl<YUVMatrix709> {};

// clang-format off
// From Re2020:
// [R]   [1.16438356164384,  0.0,                    1.678674107142860 ]   [Y -  16]
// [G] = [1.16438356164384, -0.187326104219343,     -0.650424318505057 ] x [U - 128]
// [B]   [1.16438356164384,  2.14177232142857,       0.0               ]   [V - 128]
// clang-format on
static constexpr double YUVMatrix2020[4] = {
    1.678674107142860, -0.187326104219343, -0.650424318505057,
    2.14177232142857};
template <>
struct YUVConverter<REC_2020> : YUVConverterImpl<YUVMatrix2020> {};

// clang-format off
// [R]   [V]
// [G] = [Y]
// [B]   [U]
// clang-format on
template <>
struct YUVConverter<IDENTITY> {
  static inline PackedRGBA8 convert(V8<int16_t> y, V8<int16_t> uv) {
    // Map U/V directly to B/R and map Y directly to G with opaque alpha.
    return packYUV(y, uv);
  }
};

// Helper function for textureLinearRowR8 that samples horizontal taps and
// combines them based on Y fraction with next row.
template <typename S>
static ALWAYS_INLINE V8<int16_t> linearRowTapsR8(S sampler, I32 ix,
                                                 int32_t offsety,
                                                 int32_t stridey,
                                                 int16_t fracy) {
  uint8_t* buf = (uint8_t*)sampler->buf + offsety;
  auto a0 = unaligned_load<V2<uint8_t>>(&buf[ix.x]);
  auto b0 = unaligned_load<V2<uint8_t>>(&buf[ix.y]);
  auto c0 = unaligned_load<V2<uint8_t>>(&buf[ix.z]);
  auto d0 = unaligned_load<V2<uint8_t>>(&buf[ix.w]);
  auto abcd0 = CONVERT(combine(combine(a0, b0), combine(c0, d0)), V8<int16_t>);
  buf += stridey;
  auto a1 = unaligned_load<V2<uint8_t>>(&buf[ix.x]);
  auto b1 = unaligned_load<V2<uint8_t>>(&buf[ix.y]);
  auto c1 = unaligned_load<V2<uint8_t>>(&buf[ix.z]);
  auto d1 = unaligned_load<V2<uint8_t>>(&buf[ix.w]);
  auto abcd1 = CONVERT(combine(combine(a1, b1), combine(c1, d1)), V8<int16_t>);
  abcd0 += ((abcd1 - abcd0) * fracy) >> 7;
  return abcd0;
}

// Optimized version of textureLinearPackedR8 for Y R8 texture. This assumes
// constant Y and returns a duplicate of the result interleaved with itself
// to aid in later YUV transformation.
template <typename S>
static inline V8<int16_t> textureLinearRowR8(S sampler, I32 ix, int32_t offsety,
                                             int32_t stridey, int16_t fracy) {
  assert(sampler->format == TextureFormat::R8);

  // Calculate X fraction and clamp X offset into range.
  I32 fracx = ix & (I32)0x7F;
  ix >>= 7;
  fracx &= (ix >= 0 && ix < int32_t(sampler->width) - 1);
  ix = clampCoord(ix, sampler->width);

  // Load the sample taps and combine rows.
  auto abcd = linearRowTapsR8(sampler, ix, offsety, stridey, fracy);

  // Unzip the result and do final horizontal multiply-add base on X fraction.
  auto abcdl = SHUFFLE(abcd, abcd, 0, 0, 2, 2, 4, 4, 6, 6);
  auto abcdh = SHUFFLE(abcd, abcd, 1, 1, 3, 3, 5, 5, 7, 7);
  abcdl += ((abcdh - abcdl) * CONVERT(fracx, I16).xxyyzzww) >> 7;

  // The final result is the packed values interleaved with a duplicate of
  // themselves.
  return abcdl;
}

// Optimized version of textureLinearPackedR8 for paired U/V R8 textures.
// Since the two textures have the same dimensions and stride, the addressing
// math can be shared between both samplers. This also allows a coalesced
// multiply in the final stage by packing both U/V results into a single
// operation.
template <typename S>
static inline V8<int16_t> textureLinearRowPairedR8(S sampler, S sampler2,
                                                   I32 ix, int32_t offsety,
                                                   int32_t stridey,
                                                   int16_t fracy) {
  assert(sampler->format == TextureFormat::R8 &&
         sampler2->format == TextureFormat::R8);
  assert(sampler->width == sampler2->width &&
         sampler->height == sampler2->height);
  assert(sampler->stride == sampler2->stride);

  // Calculate X fraction and clamp X offset into range.
  I32 fracx = ix & (I32)0x7F;
  ix >>= 7;
  fracx &= (ix >= 0 && ix < int32_t(sampler->width) - 1);
  ix = clampCoord(ix, sampler->width);

  // Load the sample taps for the first sampler and combine rows.
  auto abcd = linearRowTapsR8(sampler, ix, offsety, stridey, fracy);

  // Load the sample taps for the second sampler and combine rows.
  auto xyzw = linearRowTapsR8(sampler2, ix, offsety, stridey, fracy);

  // We are left with a result vector for each sampler with values for adjacent
  // pixels interleaved together in each. We need to unzip these values so that
  // we can do the final horizontal multiply-add based on the X fraction.
  auto abcdxyzwl = SHUFFLE(abcd, xyzw, 0, 8, 2, 10, 4, 12, 6, 14);
  auto abcdxyzwh = SHUFFLE(abcd, xyzw, 1, 9, 3, 11, 5, 13, 7, 15);
  abcdxyzwl += ((abcdxyzwh - abcdxyzwl) * CONVERT(fracx, I16).xxyyzzww) >> 7;

  // The final result is the packed values for the first sampler interleaved
  // with the packed values for the second sampler.
  return abcdxyzwl;
}

template <YUVColorSpace COLOR_SPACE>
static void linear_row_yuv(uint32_t* dest, int span, const vec2_scalar& srcUV,
                           float srcDU, const vec2_scalar& chromaUV,
                           float chromaDU, sampler2D_impl sampler[3]) {
  // Calculate varying and constant interp data for Y plane.
  I32 yU = cast(init_interp(srcUV.x, srcDU));
  int32_t yV = int32_t(srcUV.y);
  int16_t yFracV = yV & 0x7F;
  yV >>= 7;
  int32_t yOffsetV = clampCoord(yV, sampler[0].height) * sampler[0].stride;
  int32_t yStrideV =
      yV >= 0 && yV < int32_t(sampler[0].height) - 1 ? sampler[0].stride : 0;

  // Calculate varying and constant interp data for chroma planes.
  I32 cU = cast(init_interp(chromaUV.x, chromaDU));
  int32_t cV = int32_t(chromaUV.y);
  int16_t cFracV = cV & 0x7F;
  cV >>= 7;
  int32_t cOffsetV = clampCoord(cV, sampler[1].height) * sampler[1].stride;
  int32_t cStrideV =
      cV >= 0 && cV < int32_t(sampler[1].height) - 1 ? sampler[1].stride : 0;

  int32_t yDU = int32_t(4 * srcDU);
  int32_t cDU = int32_t(4 * chromaDU);
  for (; span >= 4; span -= 4) {
    // Sample each YUV plane and then transform them by the appropriate color
    // space.
    auto yPx = textureLinearRowR8(&sampler[0], yU, yOffsetV, yStrideV, yFracV);
    auto uvPx = textureLinearRowPairedR8(&sampler[1], &sampler[2], cU, cOffsetV,
                                         cStrideV, cFracV);
    unaligned_store(dest, YUVConverter<COLOR_SPACE>::convert(yPx, uvPx));
    dest += 4;
    yU += yDU;
    cU += cDU;
  }
  if (span > 0) {
    // Handle any remaining pixels...
    auto yPx = textureLinearRowR8(&sampler[0], yU, yOffsetV, yStrideV, yFracV);
    auto uvPx = textureLinearRowPairedR8(&sampler[1], &sampler[2], cU, cOffsetV,
                                         cStrideV, cFracV);
    auto srcpx = YUVConverter<COLOR_SPACE>::convert(yPx, uvPx);
    auto mask = span_mask_RGBA8(span);
    auto dstpx = unaligned_load<PackedRGBA8>(dest);
    unaligned_store(dest, (mask & dstpx) | (~mask & srcpx));
  }
}

static void linear_convert_yuv(Texture& ytex, Texture& utex, Texture& vtex,
                               YUVColorSpace colorSpace, const IntRect& srcReq,
                               Texture& dsttex, const IntRect& dstReq,
                               bool invertY, int bandOffset, int bandHeight) {
  // Compute valid dest bounds
  IntRect dstBounds = dsttex.sample_bounds(dstReq, invertY);
  // Check if sampling bounds are empty
  if (dstBounds.is_empty()) {
    return;
  }
  // Initialize samplers for source textures
  sampler2D_impl sampler[3];
  init_sampler(&sampler[0], ytex);
  init_sampler(&sampler[1], utex);
  init_sampler(&sampler[2], vtex);

  // Compute source UVs
  vec2_scalar srcUV(srcReq.x0, srcReq.y0);
  vec2_scalar srcDUV(float(srcReq.width()) / dstReq.width(),
                     float(srcReq.height()) / dstReq.height());
  // Skip to clamped source start
  srcUV += srcDUV * (vec2_scalar(dstBounds.x0, dstBounds.y0) + 0.5f);
  // Calculate separate chroma UVs for chroma planes with different scale
  vec2_scalar chromaScale(float(utex.width) / ytex.width,
                          float(utex.height) / ytex.height);
  vec2_scalar chromaUV = srcUV * chromaScale;
  vec2_scalar chromaDUV = srcDUV * chromaScale;
  // Scale UVs by lerp precision
  srcUV = linearQuantize(srcUV, 128);
  srcDUV *= 128.0f;
  chromaUV = linearQuantize(chromaUV, 128);
  chromaDUV *= 128.0f;
  // Calculate dest pointer from clamped offsets
  int destStride = dsttex.stride();
  char* dest = dsttex.sample_ptr(dstReq, dstBounds, 0, invertY);
  // Inverted Y must step downward along dest rows
  if (invertY) {
    destStride = -destStride;
  }
  dest += destStride * bandOffset;
  srcUV.y += srcDUV.y * bandOffset;
  chromaUV.y += chromaDUV.y * bandOffset;
  int span = dstBounds.width();
  for (int rows = min(dstBounds.height(), bandHeight); rows > 0; rows--) {
    switch (colorSpace) {
      case REC_601:
        linear_row_yuv<REC_601>((uint32_t*)dest, span, srcUV, srcDUV.x,
                                chromaUV, chromaDUV.x, sampler);
        break;
      case REC_709:
        linear_row_yuv<REC_709>((uint32_t*)dest, span, srcUV, srcDUV.x,
                                chromaUV, chromaDUV.x, sampler);
        break;
      case REC_2020:
        linear_row_yuv<REC_2020>((uint32_t*)dest, span, srcUV, srcDUV.x,
                                 chromaUV, chromaDUV.x, sampler);
        break;
      case IDENTITY:
        linear_row_yuv<IDENTITY>((uint32_t*)dest, span, srcUV, srcDUV.x,
                                 chromaUV, chromaDUV.x, sampler);
        break;
      default:
        debugf("unknown YUV color space %d\n", colorSpace);
        assert(false);
        break;
    }
    dest += destStride;
    srcUV.y += srcDUV.y;
    chromaUV.y += chromaDUV.y;
  }
}

extern "C" {

// Extension for compositing a YUV surface represented by separate YUV planes
// to a BGRA destination. The supplied color space is used to determine the
// transform from YUV to BGRA after sampling.
void CompositeYUV(LockedTexture* lockedDst, LockedTexture* lockedY,
                  LockedTexture* lockedU, LockedTexture* lockedV,
                  YUVColorSpace colorSpace, GLint srcX, GLint srcY,
                  GLsizei srcWidth, GLsizei srcHeight, GLint dstX, GLint dstY,
                  GLsizei dstWidth, GLsizei dstHeight, GLboolean flip,
                  GLint bandOffset, GLsizei bandHeight) {
  if (!lockedDst || !lockedY || !lockedU || !lockedV) {
    return;
  }
  Texture& ytex = *lockedY;
  Texture& utex = *lockedU;
  Texture& vtex = *lockedV;
  Texture& dsttex = *lockedDst;
  // All YUV planes must currently be represented by R8 textures.
  // The chroma (U/V) planes must have matching dimensions.
  assert(ytex.bpp() == 1 && utex.bpp() == 1 && vtex.bpp() == 1);
  // assert(ytex.width == utex.width && ytex.height == utex.height);
  assert(utex.width == vtex.width && utex.height == vtex.height);
  assert(dsttex.bpp() == 4);

  IntRect srcReq = {srcX, srcY, srcX + srcWidth, srcY + srcHeight};
  IntRect dstReq = {dstX, dstY, dstX + dstWidth, dstY + dstHeight};
  // For now, always use a linear filter path that would be required for
  // scaling. Further fast-paths for non-scaled video might be desirable in the
  // future.
  linear_convert_yuv(ytex, utex, vtex, colorSpace, srcReq, dsttex, dstReq, flip,
                     bandOffset, bandHeight);
}

}  // extern "C"
