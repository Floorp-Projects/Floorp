/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GL_COLORSPACES_H_
#define MOZILLA_GFX_GL_COLORSPACES_H_

// Reference: https://hackmd.io/0wkiLmP7RWOFjcD13M870A

// We are going to be doing so, so many transforms, so descriptive labels are
// critical.

// Colorspace background info: https://hackmd.io/0wkiLmP7RWOFjcD13M870A

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "AutoMappable.h"
#include "mozilla/Attributes.h"

#ifdef DEBUG
  #define ASSERT(EXPR) do { \
      if (!(EXPR)) { \
        __builtin_trap(); \
      } \
    } while (false)
#else
  #define ASSERT(EXPR) (void)(EXPR)
#endif

namespace mozilla::color {

struct YuvLumaCoeffs final {
   float r = 0.2126;
   float g = 0.7152;
   float b = 0.0722;

   auto Members() const {
      return std::tie(r, g, b);
   }
   INLINE_AUTO_MAPPABLE(YuvLumaCoeffs)

   static constexpr auto Rec709() {
      return YuvLumaCoeffs();
   }

   static constexpr auto Rec2020() {
      return YuvLumaCoeffs{0.2627, 0.6780, 0.0593};
   }
};

struct PiecewiseGammaDesc final {
   // tf = { k * linear                   | linear < b
   //      { a * pow(linear, 1/g) - (1-a) | linear >= b

   // Default to Srgb
   float a = 1.055;
   float b = 0.04045 / 12.92;
   float g = 2.4;
   float k = 12.92;

   auto Members() const {
      return std::tie(a, b, g, k);
   }
   INLINE_AUTO_MAPPABLE(PiecewiseGammaDesc)

   static constexpr auto Srgb() {
      return PiecewiseGammaDesc();
   }
   static constexpr auto DisplayP3() { return Srgb(); }

   static constexpr auto Rec709() {
      return PiecewiseGammaDesc{
         1.099,
         0.018,
         1.0 / 0.45, // ~2.222
         4.5,
      };
   }
   static constexpr auto Rec2020_10bit() {
      return Rec709();
   }

   static constexpr auto Rec2020_12bit() {
      return PiecewiseGammaDesc{
         1.0993,
         0.0181,
         1.0 / 0.45, // ~2.222
         4.5,
      };
   }
};

struct YcbcrDesc final {
   float y0 = 16 / 255.0;
   float y1 = 235 / 255.0;
   float u0 = 128 / 255.0;
   float uPlusHalf = 240 / 255.0;

   auto Members() const {
      return std::tie(y0, y1, u0, uPlusHalf);
   }
   INLINE_AUTO_MAPPABLE(YcbcrDesc)

   static constexpr auto Narrow8() { // AKA limited/studio/tv
      return YcbcrDesc();
   }
   static constexpr auto Full8() { // AKA pc
      return YcbcrDesc{
         0 / 255.0,
         255 / 255.0,
         128 / 255.0,
         254 / 255.0,
      };
   }
   static constexpr auto Float() { // Best for a LUT
      return YcbcrDesc{0.0, 1.0, 0.5, 1.0};
   }
};

struct Chromaticities final {
   float rx = 0.640;
   float ry = 0.330;
   float gx = 0.300;
   float gy = 0.600;
   float bx = 0.150;
   float by = 0.060;
   // D65:
   static constexpr float wx = 0.3127;
   static constexpr float wy = 0.3290;

   auto Members() const {
      return std::tie(rx, ry, gx, gy, bx, by);
   }
   INLINE_AUTO_MAPPABLE(Chromaticities)

   // -

   static constexpr auto Rec709() { // AKA limited/studio/tv
      return Chromaticities();
   }
   static constexpr auto Srgb() { return Rec709(); }

   static constexpr auto Rec601_625_Pal() {
      auto ret = Rec709();
      ret.gx = 0.290;
      return ret;
   }
   static constexpr auto Rec601_525_Ntsc() {
      return Chromaticities{
         0.630, 0.340, // r
         0.310, 0.595, // g
         0.155, 0.070, // b
      };
   }
   static constexpr auto Rec2020() {
      return Chromaticities{
         0.708, 0.292, // r
         0.170, 0.797, // g
         0.131, 0.046, // b
      };
   }
   static constexpr auto DisplayP3() {
      return Chromaticities{
         0.680, 0.320, // r
         0.265, 0.690, // g
         0.150, 0.060, // b
      };
   }
};

// -

struct YuvDesc final {
   YuvLumaCoeffs yCoeffs;
   YcbcrDesc ycbcr;

   auto Members() const {
      return std::tie(yCoeffs, ycbcr);
   }
   INLINE_AUTO_MAPPABLE(YuvDesc);
};

struct ColorspaceDesc final {
   Chromaticities chrom;
   std::optional<PiecewiseGammaDesc> tf;
   std::optional<YuvDesc> yuv;

   auto Members() const {
      return std::tie(chrom, tf, yuv);
   }
   INLINE_AUTO_MAPPABLE(ColorspaceDesc);
};

// -

template<class TT, int NN>
struct avec final {
   using T = TT;
   static constexpr auto N = NN;

   std::array<T,N> data = {};

   // -

   constexpr avec() = default;
   constexpr avec(const avec&) = default;

   constexpr avec(const avec<T, N-1>& v, T a) {
      for (int i = 0; i < N-1; i++) {
         data[i] = v[i];
      }
      data[N-1] = a;
   }
   constexpr avec(const avec<T, N-2>& v, T a, T b) {
      for (int i = 0; i < N-2; i++) {
         data[i] = v[i];
      }
      data[N-2] = a;
      data[N-1] = b;
   }

   MOZ_IMPLICIT constexpr avec(const std::array<T,N>& data) {
      this->data = data;
   }

   explicit constexpr avec(const T v) {
      for (int i = 0; i < N; i++) {
         data[i] = v;
      }
   }

   template<class T2, int N2>
   explicit constexpr avec(const avec<T2, N2>& v) {
      const auto n = std::min(N, N2);
      for (int i = 0; i < n; i++) {
         data[i] = static_cast<T>(v[i]);
      }
   }

   // -

   const auto& operator[](const size_t n) const { return data[n]; }
   auto& operator[](const size_t n) { return data[n]; }

   template<int i>
   constexpr auto get() const {
      return (i < N) ? data[i] : 0;
   }
   constexpr auto x() const { return get<0>(); }
   constexpr auto y() const { return get<1>(); }
   constexpr auto z() const { return get<2>(); }
   constexpr auto w() const { return get<3>(); }

   constexpr auto xyz() const {
      return vec3({x(), y(), z()});
   }

   template<int i>
   void set(const T v) {
      if (i < N) {
         data[i] = v;
      }
   }
   void x(const T v) { set<0>(v); }
   void y(const T v) { set<1>(v); }
   void z(const T v) { set<2>(v); }
   void w(const T v) { set<3>(v); }

   // -

   #define _(OP) \
      friend avec operator OP(const avec a, const avec b) { \
         avec c; \
         for (int i = 0; i < N; i++) { \
            c[i] = a[i] OP b[i]; \
         } \
         return c; \
      } \
      friend avec operator OP(const avec a, const T b) { \
         avec c; \
         for (int i = 0; i < N; i++) { \
            c[i] = a[i] OP b; \
         } \
         return c; \
      } \
      friend avec operator OP(const T a, const avec b) { \
         avec c; \
         for (int i = 0; i < N; i++) { \
            c[i] = a OP b[i]; \
         } \
         return c; \
      }
   _(+)
   _(-)
   _(*)
   _(/)
   #undef _

   friend bool operator==(const avec a, const avec b) {
      bool eq = true;
      for (int i = 0; i < N; i++) {
         eq &= (a[i] == b[i]);
      }
      return eq;
   }
};
using vec3 = avec<float, 3>;
using vec4 = avec<float, 4>;
using ivec3 = avec<int32_t, 3>;
using ivec4 = avec<int32_t, 4>;

template<class T, int N>
T dot(const avec<T, N>& a, const avec<T, N>& b) {
   const auto c = a * b;
   T ret = 0;
   for (int i = 0; i < N; i++) {
      ret += c[i];
   }
   return ret;
}

template<class V>
V mix(const V& zero, const V& one, const float val) {
   return zero * (1 - val) + one * val;
}

template<class T, int N>
auto min(const avec<T, N>& a, const avec<T, N>& b) {
   auto ret = avec<T, N>{};
   for (int i = 0; i < ret.N; i++) {
      ret[i] = std::min(a[i], b[i]);
   }
   return ret;
}

template<class T, int N>
auto max(const avec<T, N>& a, const avec<T, N>& b) {
   auto ret = avec<T, N>{};
   for (int i = 0; i < ret.N; i++) {
      ret[i] = std::max(a[i], b[i]);
   }
   return ret;
}

template<class T, int N>
auto floor(const avec<T, N>& a) {
   auto ret = avec<T, N>{};
   for (int i = 0; i < ret.N; i++) {
      ret[i] = floorf(a[i]);
   }
   return ret;
}

template<class T, int N>
auto round(const avec<T, N>& a) {
   auto ret = avec<T, N>{};
   for (int i = 0; i < ret.N; i++) {
      ret[i] = roundf(a[i]);
   }
   return ret;
}

template<class T>
inline auto t_abs(const T& a) {
   return abs(a);
}
template<>
inline auto t_abs<float>(const float& a) {
   return fabs(a);
}

template<class T, int N>
auto abs(const avec<T, N>& a) {
   auto ret = avec<T, N>{};
   for (int i = 0; i < ret.N; i++) {
      ret[i] = t_abs(a[i]);
   }
   return ret;
}

// -

template<int Y_Rows, int X_Cols>
struct mat final {
   static constexpr int y_rows = Y_Rows;
   static constexpr int x_cols = X_Cols;

   static constexpr auto Identity() {
      auto ret = mat{};
      for (int x = 0; x < x_cols; x++) {
         for (int y = 0; y < y_rows; y++) {
            ret.at(x,y) = (x == y ? 1 : 0);
         }
      }
      return ret;
   }

   std::array<avec<float, X_Cols>, Y_Rows> rows = {}; // row-major

   // -

   constexpr mat() {}

   explicit constexpr mat(const std::array<avec<float, X_Cols>, Y_Rows>& rows) {
      this->rows = rows;
   }

   template<int Y_Rows2, int X_Cols2>
   explicit constexpr mat(const mat<Y_Rows2, X_Cols2>& m) {
      *this = Identity();
      for (int x = 0; x < std::min(X_Cols, X_Cols2); x++) {
         for (int y = 0; y < std::min(Y_Rows, Y_Rows2); y++) {
            at(x,y) = m.at(x, y);
         }
      }
   }

   const auto& at(const int x, const int y) const { return rows.at(y)[x]; }
   auto& at(const int x, const int y) { return rows.at(y)[x]; }

   friend auto operator*(const mat& a,
                         const avec<float, X_Cols>& b_colvec) {
      avec<float, Y_Rows> c_colvec;
      for (int i = 0; i < y_rows; i++) {
         c_colvec[i] = dot(a.rows.at(i), b_colvec);
      }
      return c_colvec;
   }

   friend auto operator*(const mat& a, const float b) {
      mat c;
      for (int x = 0; x < x_cols; x++) {
         for (int y = 0; y < y_rows; y++) {
            c.at(x,y) = a.at(x,y) * b;
         }
      }
      return c;
   }
   friend auto operator/(const mat& a, const float b) {
      return a * (1 / b);
   }

   template<int BCols, int BRows = X_Cols>
   friend auto operator*(const mat& a,
                         const mat<BRows, BCols>& b) {
      const auto bt = transpose(b);
      const auto& b_cols = bt.rows;

      mat<Y_Rows, BCols> c;
      for (int x = 0; x < BCols; x++) {
         for (int y = 0; y < Y_Rows; y++) {
            c.at(x,y) = dot(a.rows.at(y), b_cols.at(x));
         }
      }
      return c;
   }
};
using mat3 = mat<3,3>;
using mat4 = mat<4,4>;

inline float determinant(const mat<1,1>& m) {
  return m.at(0,0);
}
template<class T>
float determinant(const T& m) {
  static_assert(T::x_cols == T::y_rows);

  float ret = 0;
  for (int i = 0; i < T::x_cols; i++) {
    const auto cofact = cofactor(m, i, 0);
    ret += m.at(i, 0) * cofact;
  }
  return ret;
}

// -

template<class T>
float cofactor(const T& m, const int x_col, const int y_row) {
  ASSERT(0 <= x_col && x_col < T::x_cols);
  ASSERT(0 <= y_row && y_row < T::y_rows);

  auto cofactor = minor_val(m, x_col, y_row);
  if ((x_col+y_row) % 2 == 1) {
    cofactor *= -1;
  }
  return cofactor;
}

// -

// Unfortunately, can't call this `minor(...)` because there is
// `#define minor(dev) gnu_dev_minor (dev)`
// in /usr/include/x86_64-linux-gnu/sys/sysmacros.h:62
template<class T>
float minor_val(const T& a, const int skip_x, const int skip_y) {
  ASSERT(0 <= skip_x && skip_x < T::x_cols);
  ASSERT(0 <= skip_y && skip_y < T::y_rows);

  // A minor matrix is a matrix without its x_col and y_row.
  mat<T::y_rows-1, T::x_cols-1> b;

  int x_skips = 0;
  for (int ax = 0; ax < T::x_cols; ax++) {
    if (ax == skip_x) {
      x_skips = 1;
      continue;
    }

    int y_skips = 0;
    for (int ay = 0; ay < T::y_rows; ay++) {
      if (ay == skip_y) {
        y_skips = 1;
        continue;
      }

      b.at(ax - x_skips, ay - y_skips) = a.at(ax, ay);
    }
  }

  const auto minor = determinant(b);
  return minor;
}

// -

/// The matrix of cofactors.
template<class T>
auto comatrix(const T& a) {
  auto b = T{};
  for (int x = 0; x < T::x_cols; x++) {
    for (int y = 0; y < T::y_rows; y++) {
      b.at(x, y) = cofactor(a, x, y);
    }
  }
  return b;
}

// -

template<class T>
auto transpose(const T& a) {
  auto b = mat<T::x_cols, T::y_rows>{};
  for (int x = 0; x < T::x_cols; x++) {
    for (int y = 0; y < T::y_rows; y++) {
      b.at(y, x) = a.at(x, y);
    }
  }
  return b;
}

// -

template<class T>
inline T inverse(const T& a) {
  const auto det = determinant(a);
  const auto comat = comatrix(a);
  const auto adjugate = transpose(comat);
  const auto inv = adjugate / det;
  return inv;
}

// -

template<class F>
void ForEachIntWithin(const ivec3 size, const F& f) {
    ivec3 p;
    for (p.z(0); p.z() < size.z(); p.z(p.z()+1)) {
        for (p.y(0); p.y() < size.y(); p.y(p.y()+1)) {
            for (p.x(0); p.x() < size.x(); p.x(p.x()+1)) {
                f(p);
            }
        }
    }
}
template<class F>
void ForEachSampleWithin(const ivec3 size, const F& f) {
   const auto div = vec3(size - 1);
    ForEachIntWithin(size, [&](const ivec3& isrc) {
      const auto fsrc = vec3(isrc) / div;
      f(fsrc);
   });
}

// -

struct Lut3 final {
   ivec3 size;
   std::vector<vec3> data;

   // -

   static Lut3 Create(const ivec3 size) {
      Lut3 lut;
      lut.size = size;
      lut.data.resize(size.x() * size.y() * size.z());
      return lut;
   }

   // -

   /// p: [0, N-1] (clamps)
   size_t Index(ivec3 p) const {
      const auto scales = ivec3({
         1,
         size.x(),
         size.x() * size.y()
      });
      p = max(ivec3(0), min(p, size - 1)); // clamp
      return dot(p, scales);
   }

   // -

   template<class F>
   void SetMap(const F& dstFromSrc01) {
       ForEachIntWithin(size, [&](const ivec3 p) {
           const auto i = Index(p);
            const auto src01 = vec3(p) / vec3(size - 1);
            const auto dstVal = dstFromSrc01(src01);
            data.at(i) = dstVal;
       });
   }

   // -

   /// p: [0, N-1] (clamps)
   vec3 Fetch(ivec3 p) const {
      const auto i = Index(p);
      return data.at(i);
   }

   /// in01: [0.0, 1.0] (clamps)
   vec3 Sample(vec3 in01) const;
};

// -

/**
Naively, it would be ideal to map directly from ycbcr to rgb,
but headroom and footroom are problematic: For e.g. narrow-range-8-bit,
our naive LUT would start at absolute y=0/255. However, values only start
at y=16/255, and depending on where your first LUT sample is, you might get
very poor approximations for y=16/255.
Further, even for full-range-8-bit, y=-0.5 is encoded as 1/255. U and v
aren't *as* important as y, but we should try be accurate for the min and
max values. Additionally, it would be embarassing to get whites/greys wrong,
so preserving u=0.0 should also be a goal.
Finally, when using non-linear transfer functions, the linear approximation of a
point between two samples will be fairly inaccurate.
We preserve min and max by choosing our input range such that min and max are
the endpoints of their LUT axis.
We preserve accuracy (at and around) mid by choosing odd sizes for dimentions.

But also, the LUT is surprisingly robust, so check if the simple version works
before adding complexity!
**/

struct ColorspaceTransform final {
    ColorspaceDesc srcSpace;
    ColorspaceDesc dstSpace;
    mat4 srcRgbTfFromSrc;
    std::optional<PiecewiseGammaDesc> srcTf;
    mat3 dstRgbLinFromSrcRgbLin;
    std::optional<PiecewiseGammaDesc> dstTf;
    mat4 dstFromDstRgbTf;

    static ColorspaceTransform Create(const ColorspaceDesc& src,
                                      const ColorspaceDesc& dst);

    // -

    vec3 DstFromSrc(vec3 src) const;

    std::optional<mat4> ToMat4() const;

    Lut3 ToLut3(const ivec3 size) const;
    Lut3 ToLut3() const {
      auto defaultSize = ivec3({31,31,15}); // Order of importance: G, R, B
      if (srcSpace.yuv) {
         defaultSize = ivec3({31,15,31}); // Y, Cb, Cr
      }
      return ToLut3(defaultSize);
   }
};

}  // namespace mozilla::color

#undef ASSERT

#endif  // MOZILLA_GFX_GL_COLORSPACES_H_
