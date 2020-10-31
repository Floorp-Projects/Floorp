/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

static inline void commit_span(uint32_t* buf, PackedRGBA8 r) {
  if (blend_key)
    r = pack(blend_pixels_RGBA8(unaligned_load<PackedRGBA8>(buf), unpack(r)));
  unaligned_store(buf, r);
}

UNUSED static inline void commit_solid_span(uint32_t* buf, PackedRGBA8 r,
                                            int len) {
  if (blend_key) {
    auto src = unpack(r);
    for (uint32_t* end = &buf[len]; buf < end; buf += 4) {
      unaligned_store(
          buf, pack(blend_pixels_RGBA8(unaligned_load<PackedRGBA8>(buf), src)));
    }
  } else {
    fill_n(buf, len, bit_cast<U32>(r).x);
  }
}

UNUSED static inline void commit_texture_span(uint32_t* buf, uint32_t* src,
                                              int len) {
  if (blend_key) {
    for (uint32_t* end = &buf[len]; buf < end; buf += 4, src += 4) {
      PackedRGBA8 r = unaligned_load<PackedRGBA8>(src);
      unaligned_store(buf, pack(blend_pixels_RGBA8(
                               unaligned_load<PackedRGBA8>(buf), unpack(r))));
    }
  } else {
    memcpy(buf, src, len * sizeof(uint32_t));
  }
}

static inline void commit_span(uint8_t* buf, PackedR8 r) {
  if (blend_key)
    r = pack(blend_pixels_R8(unpack(unaligned_load<PackedR8>(buf)), unpack(r)));
  unaligned_store(buf, r);
}

UNUSED static inline void commit_solid_span(uint8_t* buf, PackedR8 r, int len) {
  if (blend_key) {
    auto src = unpack(r);
    for (uint8_t* end = &buf[len]; buf < end; buf += 4) {
      unaligned_store(buf, pack(blend_pixels_R8(
                               unpack(unaligned_load<PackedR8>(buf)), src)));
    }
  } else {
    fill_n((uint32_t*)buf, len / 4, bit_cast<uint32_t>(r));
  }
}

template <typename V>
static inline PackedRGBA8 pack_span(uint32_t*, const V& v) {
  return pack(pack_pixels_RGBA8(v));
}

static inline PackedRGBA8 pack_span(uint32_t*) {
  return pack(pack_pixels_RGBA8());
}

template <typename C>
static inline PackedR8 pack_span(uint8_t*, C c) {
  return pack(pack_pixels_R8(c));
}

static inline PackedR8 pack_span(uint8_t*) { return pack(pack_pixels_R8()); }

// Forces a value with vector run-class to have scalar run-class.
template <typename T>
static ALWAYS_INLINE auto swgl_forceScalar(T v) -> decltype(force_scalar(v)) {
  return force_scalar(v);
}

// Advance all varying inperpolants by a single chunk
#define swgl_stepInterp() step_interp_inputs()

// Pseudo-intrinsic that accesses the interpolation step for a given varying
#define swgl_interpStep(v) (interp_step.v)

// Commit an entire span of a solid color
#define swgl_commitSolid(format, v)                                       \
  do {                                                                    \
    commit_solid_span(swgl_Out##format, pack_span(swgl_Out##format, (v)), \
                      swgl_SpanLength);                                   \
    swgl_Out##format += swgl_SpanLength;                                  \
    swgl_SpanLength = 0;                                                  \
  } while (0)
#define swgl_commitSolidRGBA8(v) swgl_commitSolid(RGBA8, v)
#define swgl_commitSolidR8(v) swgl_commitSolid(R8, v)

#define swgl_commitChunk(format, chunk)   \
  do {                                    \
    commit_span(swgl_Out##format, chunk); \
    swgl_Out##format += swgl_StepSize;    \
    swgl_SpanLength -= swgl_StepSize;     \
  } while (0)

static inline WideRGBA8 pack_pixels_RGBA8(Float alpha) {
  I32 i = round_pixel(alpha);
  HalfRGBA8 c = packRGBA8(zipLow(i, i), zipHigh(i, i));
  return combine(zipLow(c, c), zipHigh(c, c));
}

// Commit a single chunk of a color scaled by an alpha weight
#define swgl_commitColor(format, color, alpha)                         \
  swgl_commitChunk(format, pack(muldiv255(pack_pixels_##format(color), \
                                          pack_pixels_##format(alpha))))
#define swgl_commitColorRGBA8(color, alpha) \
  swgl_commitColor(RGBA8, color, alpha)
#define swgl_commitColorR8(color, alpha) swgl_commitColor(R8, color, alpha)

template <typename S>
static ALWAYS_INLINE bool swgl_isTextureLinear(S s) {
  return s->filter == TextureFilter::LINEAR;
}

template <typename S>
static ALWAYS_INLINE bool swgl_isTextureRGBA8(S s) {
  return s->format == TextureFormat::RGBA8;
}

template <typename S>
static ALWAYS_INLINE bool swgl_isTextureR8(S s) {
  return s->format == TextureFormat::R8;
}

// Returns the offset into the texture buffer for the given layer index. If not
// a texture array or 3D texture, this will always access the first layer.
template <typename S>
static ALWAYS_INLINE int swgl_textureLayerOffset(S s, float layer) {
  return 0;
}

UNUSED static ALWAYS_INLINE int swgl_textureLayerOffset(sampler2DArray s,
                                                        float layer) {
  return clampCoord(int(layer), s->depth) * s->height_stride;
}

// Use the default linear quantization scale of 128. This gives 7 bits of
// fractional precision, which when multiplied with a signed 9 bit value
// still fits in a 16 bit integer.
const int swgl_LinearQuantizeScale = 128;

// Quantizes UVs for access into a linear texture.
template <typename S, typename T>
static ALWAYS_INLINE T swgl_linearQuantize(S s, T p) {
  return linearQuantize(p, swgl_LinearQuantizeScale, s);
}

// Quantizes an interpolation step for UVs for access into a linear texture.
template <typename S, typename T>
static ALWAYS_INLINE T swgl_linearQuantizeStep(S s, T p) {
  return samplerScale(s, p) * swgl_LinearQuantizeScale;
}

// Commit a single chunk from a linear texture fetch
#define swgl_commitTextureLinear(format, s, p, ...) \
  swgl_commitChunk(format,                          \
                   textureLinearPacked##format(s, ivec2(p), __VA_ARGS__))
#define swgl_commitTextureLinearRGBA8(s, p, ...) \
  swgl_commitTextureLinear(RGBA8, s, p, __VA_ARGS__)
#define swgl_commitTextureLinearR8(s, p, ...) \
  swgl_commitTextureLinear(R8, s, p, __VA_ARGS__)

// Commit a single chunk from a linear texture fetch that is scaled by a color
#define swgl_commitTextureLinearColor(format, s, p, color, ...)               \
  swgl_commitChunk(                                                           \
      format,                                                                 \
      pack(muldiv255(textureLinearUnpacked##format(s, ivec2(p), __VA_ARGS__), \
                     pack_pixels_##format(color))))
#define swgl_commitTextureLinearColorRGBA8(s, p, color, ...) \
  swgl_commitTextureLinearColor(RGBA8, s, p, color, __VA_ARGS__)
#define swgl_commitTextureLinearColorR8(s, p, color, ...) \
  swgl_commitTextureLinearColor(R8, s, p, color, __VA_ARGS__)

// Dispatch helper used by the GLSL translator to swgl_drawSpan functions.
// The number of pixels committed is tracked by checking for the difference in
// swgl_SpanLength. Any varying interpolants used will be advanced past the
// committed part of the span in case the fragment shader must be executed for
// any remaining pixels that were not committed by the span shader.
#define DISPATCH_DRAW_SPAN(self, format)                                      \
  do {                                                                        \
    int total = self->swgl_SpanLength;                                        \
    self->swgl_drawSpan##format();                                            \
    int drawn = total - self->swgl_SpanLength;                                \
    if (drawn) self->step_interp_inputs(drawn);                               \
    while (self->swgl_SpanLength > 0) {                                       \
      run(self);                                                              \
      commit_span(self->swgl_Out##format, pack_span(self->swgl_Out##format)); \
      self->swgl_Out##format += swgl_StepSize;                                \
      self->swgl_SpanLength -= swgl_StepSize;                                 \
    }                                                                         \
  } while (0)
