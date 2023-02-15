/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We are going to be doing so, so many transforms, so descriptive labels are
// critical.

#include "Colorspaces.h"

namespace mozilla::color {

// tf = { k * linear                   | linear < b
//      { a * pow(linear, 1/g) - (1-a) | linear >= b
float TfFromLinear(const PiecewiseGammaDesc& desc, const float linear) {
  if (linear < desc.b) {
    return linear * desc.k;
  }
  float ret = linear;
  ret = powf(ret, 1.0f / desc.g);
  ret *= desc.a;
  ret -= (desc.a - 1);
  return ret;
}

float LinearFromTf(const PiecewiseGammaDesc& desc, const float tf) {
  const auto linear_if_low = tf / desc.k;
  if (linear_if_low < desc.b) {
    return linear_if_low;
  }
  float ret = tf;
  ret += (desc.a - 1);
  ret /= desc.a;
  ret = powf(ret, 1.0f * desc.g);
  return ret;
}

// -

mat3 YuvFromRgb(const YuvLumaCoeffs& yc) {
  // Y is always [0,1]
  // U and V are signed, and could be either [-1,+1] or [-0.5,+0.5].
  // Specs generally use [-0.5,+0.5], so we use that too.
  // E.g.
  // y = 0.2126*r + 0.7152*g + 0.0722*b
  // u = (b - y) / (u_range = u_max - u_min) // u_min = -u_max
  //   = (b - y) / (u(0,0,1) - u(1,1,0))
  //   = (b - y) / (2 * u(0,0,1))
  //   = (b - y) / (2 * u.b))
  //   = (b - y) / (2 * (1 - 0.0722))
  //   = (-0.2126*r + -0.7152*g + (1-0.0722)*b) / 1.8556
  // v = (r - y) / 1.5748;
  //   = ((1-0.2126)*r + -0.7152*g + -0.0722*b) / 1.5748
  const auto y = vec3({yc.r, yc.g, yc.b});
  const auto u = vec3({0, 0, 1}) - y;
  const auto v = vec3({1, 0, 0}) - y;

  // From rows:
  return mat3({y, u / (2 * u.z()), v / (2 * v.x())});
}

mat4 YuvFromYcbcr(const YcbcrDesc& d) {
  // E.g.
  // y = (yy - 16) / (235 - 16); // 16->0, 235->1
  // u = (cb - 128) / (240 - 16); // 16->-0.5, 128->0, 240->+0.5
  // v = (cr - 128) / (240 - 16);

  const auto yRange = d.y1 - d.y0;
  const auto uHalfRange = d.uPlusHalf - d.u0;
  const auto uRange = 2 * uHalfRange;

  const auto ycbcrFromYuv = mat4{{vec4{{yRange, 0, 0, d.y0}},
                                  {{0, uRange, 0, d.u0}},
                                  {{0, 0, uRange, d.u0}},
                                  {{0, 0, 0, 1}}}};
  const auto yuvFromYcbcr = inverse(ycbcrFromYuv);
  return yuvFromYcbcr;
}

mat3 XyzFromLinearRgb(const Chromaticities& c) {
  // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html

  // Given red (xr, yr), green (xg, yg), blue (xb, yb),
  // and whitepoint (XW, YW, ZW)

  // [ X ]       [ R ]
  // [ Y ] = M x [ G ]
  // [ Z ]       [ B ]

  //     [ Sr*Xr Sg*Xg Sb*Xb ]
  // M = [ Sr*Yr Sg*Yg Sb*Yb ]
  //     [ Sr*Zr Sg*Zg Sb*Zb ]

  // Xr = xr / yr
  // Yr = 1
  // Zr = (1 - xr - yr) / yr

  // Xg = xg / yg
  // Yg = 1
  // Zg = (1 - xg - yg) / yg

  // Xb = xb / yb
  // Yb = 1
  // Zb = (1 - xb - yb) / yb

  // [ Sr ]   [ Xr Xg Xb ]^-1   [ XW ]
  // [ Sg ] = [ Yr Yg Yb ]    x [ YW ]
  // [ Sb ]   [ Zr Zg Zb ]      [ ZW ]

  const auto xrgb = vec3({c.rx, c.gx, c.bx});
  const auto yrgb = vec3({c.ry, c.gy, c.by});

  const auto Xrgb = xrgb / yrgb;
  const auto Yrgb = vec3(1);
  const auto Zrgb = (vec3(1) - xrgb - yrgb) / yrgb;

  const auto XYZrgb = mat3({Xrgb, Yrgb, Zrgb});
  const auto XYZrgb_inv = inverse(XYZrgb);
  const auto XYZwhitepoint = vec3({c.wx, c.wy, 1 - c.wx - c.wy});
  const auto Srgb = XYZrgb_inv * XYZwhitepoint;

  const auto M = mat3({Srgb * Xrgb, Srgb * Yrgb, Srgb * Zrgb});
  return M;
}

// -
ColorspaceTransform ColorspaceTransform::Create(const ColorspaceDesc& src,
                                                const ColorspaceDesc& dst) {
  auto ct = ColorspaceTransform{src, dst};
  ct.srcTf = src.tf;
  ct.dstTf = dst.tf;

  const auto RgbTfFrom = [&](const ColorspaceDesc& cs) {
    auto rgbFrom = mat4::Identity();
    if (cs.yuv) {
      const auto yuvFromYcbcr = YuvFromYcbcr(cs.yuv->ycbcr);
      const auto yuvFromRgb = YuvFromRgb(cs.yuv->yCoeffs);
      const auto rgbFromYuv = inverse(yuvFromRgb);
      const auto rgbFromYuv4 = mat4(rgbFromYuv);

      const auto rgbFromYcbcr = rgbFromYuv4 * yuvFromYcbcr;
      rgbFrom = rgbFromYcbcr;
    }
    return rgbFrom;
  };

  ct.srcRgbTfFromSrc = RgbTfFrom(src);
  const auto dstRgbTfFromDst = RgbTfFrom(dst);
  ct.dstFromDstRgbTf = inverse(dstRgbTfFromDst);

  // -

  ct.dstRgbLinFromSrcRgbLin = mat3::Identity();
  if (!(src.chrom == dst.chrom)) {
    const auto xyzFromSrcRgbLin = XyzFromLinearRgb(src.chrom);
    const auto xyzFromDstRgbLin = XyzFromLinearRgb(dst.chrom);
    const auto dstRgbLinFromXyz = inverse(xyzFromDstRgbLin);
    ct.dstRgbLinFromSrcRgbLin = dstRgbLinFromXyz * xyzFromSrcRgbLin;
  }

  return ct;
}

vec3 ColorspaceTransform::DstFromSrc(const vec3 src) const {
  const auto srcRgbTf = srcRgbTfFromSrc * vec4(src, 1);
  auto srcRgbLin = srcRgbTf;
  if (srcTf) {
    srcRgbLin.x(LinearFromTf(*srcTf, srcRgbTf.x()));
    srcRgbLin.y(LinearFromTf(*srcTf, srcRgbTf.y()));
    srcRgbLin.z(LinearFromTf(*srcTf, srcRgbTf.z()));
  }

  const auto dstRgbLin = dstRgbLinFromSrcRgbLin * vec3(srcRgbLin);
  auto dstRgbTf = dstRgbLin;
  if (dstTf) {
    dstRgbTf.x(TfFromLinear(*dstTf, dstRgbLin.x()));
    dstRgbTf.y(TfFromLinear(*dstTf, dstRgbLin.y()));
    dstRgbTf.z(TfFromLinear(*dstTf, dstRgbLin.z()));
  }

  const auto dst4 = dstFromDstRgbTf * vec4(dstRgbTf, 1);
  return vec3(dst4);
}

// -

std::optional<mat4> ColorspaceTransform::ToMat4() const {
  mat4 fromSrc = srcRgbTfFromSrc;
  if (srcTf) return {};
  fromSrc = mat4(dstRgbLinFromSrcRgbLin) * fromSrc;
  if (dstTf) return {};
  fromSrc = dstFromDstRgbTf * fromSrc;
  return fromSrc;
}

Lut3 ColorspaceTransform::ToLut3(const ivec3 size) const {
  auto lut = Lut3::Create(size);
  lut.SetMap([&](const vec3& srcVal) { return DstFromSrc(srcVal); });
  return lut;
}

vec3 Lut3::Sample(const vec3 in01) const {
  const auto coord = vec3(size - 1) * in01;
  const auto p0 = floor(coord);
  const auto dp = coord - p0;
  const auto ip0 = ivec3(p0);

  // Trilinear
  const auto f000 = Fetch(ip0 + ivec3({0, 0, 0}));
  const auto f100 = Fetch(ip0 + ivec3({1, 0, 0}));
  const auto f010 = Fetch(ip0 + ivec3({0, 1, 0}));
  const auto f110 = Fetch(ip0 + ivec3({1, 1, 0}));
  const auto f001 = Fetch(ip0 + ivec3({0, 0, 1}));
  const auto f101 = Fetch(ip0 + ivec3({1, 0, 1}));
  const auto f011 = Fetch(ip0 + ivec3({0, 1, 1}));
  const auto f111 = Fetch(ip0 + ivec3({1, 1, 1}));

  const auto fx00 = mix(f000, f100, dp.x());
  const auto fx10 = mix(f010, f110, dp.x());
  const auto fx01 = mix(f001, f101, dp.x());
  const auto fx11 = mix(f011, f111, dp.x());

  const auto fxy0 = mix(fx00, fx10, dp.y());
  const auto fxy1 = mix(fx01, fx11, dp.y());

  const auto fxyz = mix(fxy0, fxy1, dp.z());
  return fxyz;
}

}  // namespace mozilla::color
