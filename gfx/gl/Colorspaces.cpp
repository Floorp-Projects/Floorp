/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We are going to be doing so, so many transforms, so descriptive labels are
// critical.

#include "Colorspaces.h"

#include "nsDebug.h"
#include "qcms.h"

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

inline vec3 CIEXYZ_from_CIExyY(const vec2 xy, const float Y = 1) {
  const auto xyz = vec3(xy, 1 - xy.x() - xy.y());
  const auto XYZ = xyz * (Y / xy.y());
  return XYZ;
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
  const auto XYZwhitepoint = vec3({c.wx, c.wy, 1 - c.wx - c.wy}) / c.wy;
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

mat3 XyzAFromXyzB_BradfordLinear(const vec2 xyA, const vec2 xyB) {
  // This is what ICC profiles use to do whitepoint transforms,
  // because ICC also requires D50 for the Profile Connection Space.

  // From https://www.color.org/specification/ICC.1-2022-05.pdf
  // E.3 "Linearized Bradford transformation":

  const auto M_BFD = mat3{{
      vec3{{0.8951, 0.2664f, -0.1614f}},
      vec3{{-0.7502f, 1.7135f, 0.0367f}},
      vec3{{0.0389f, -0.0685f, 1.0296f}},
  }};
  // NB: They use rho/gamma/beta, but we'll use R/G/B here.
  const auto XYZDst = CIEXYZ_from_CIExyY(xyA);  // "XYZ_W", WP of PCS
  const auto XYZSrc = CIEXYZ_from_CIExyY(xyB);  // "XYZ_NAW", WP of src
  const auto rgbSrc = M_BFD * XYZSrc;           // "RGB_SRC"
  const auto rgbDst = M_BFD * XYZDst;           // "RGB_PCS"
  const auto rgbDstOverSrc = rgbDst / rgbSrc;
  const auto M_dstOverSrc = mat3::Scale(rgbDstOverSrc);
  const auto M_adapt = inverse(M_BFD) * M_dstOverSrc * M_BFD;
  return M_adapt;
}

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

// -

ColorProfileDesc ColorProfileDesc::From(const ColorspaceDesc& cspace) {
  auto ret = ColorProfileDesc{};

  if (cspace.yuv) {
    const auto yuvFromYcbcr = YuvFromYcbcr(cspace.yuv->ycbcr);
    const auto yuvFromRgb = YuvFromRgb(cspace.yuv->yCoeffs);
    const auto rgbFromYuv = inverse(yuvFromRgb);
    ret.rgbFromYcbcr = mat4(rgbFromYuv) * yuvFromYcbcr;
  }

  if (cspace.tf) {
    const size_t tableSize = 256;
    auto& tableR = ret.linearFromTf.r;
    tableR.resize(tableSize);
    for (size_t i = 0; i < tableR.size(); i++) {
      const float tfVal = i / float(tableR.size() - 1);
      const float linearVal = LinearFromTf(*cspace.tf, tfVal);
      tableR[i] = linearVal;
    }
    ret.linearFromTf.g = tableR;
    ret.linearFromTf.b = tableR;
  }

  ret.xyzd65FromLinearRgb = XyzFromLinearRgb(cspace.chrom);

  return ret;
}

// -

template <class T>
constexpr inline T NewtonEstimateX(const T x1, const T y1, const T dydx,
                                   const T y2 = 0) {
  // Estimate x s.t. y=0
  // y = y0 + x*dydx;
  // y0 = y - x*dydx;
  // y1 - x1*dydx = y2 - x2*dydx
  // x2*dydx = y2 - y1 + x1*dydx
  // x2 = (y2 - y1)/dydx + x1
  return (y2 - y1) / dydx + x1;
}

float GuessGamma(const std::vector<float>& vals, float exp_guess) {
  // Approximate (signed) error = 0.0.
  constexpr float d_exp = 0.001;
  constexpr float error_tolerance = 0.001;
  struct Samples {
    float y1, y2;
  };
  const auto Sample = [&](const float exp) {
    int i = -1;
    auto samples = Samples{};
    for (const auto& expected : vals) {
      i += 1;
      const auto in = i / float(vals.size() - 1);
      samples.y1 += powf(in, exp) - expected;
      samples.y2 += powf(in, exp + d_exp) - expected;
    }
    samples.y1 /= vals.size();  // Normalize by val count.
    samples.y2 /= vals.size();
    return samples;
  };
  constexpr int MAX_ITERS = 10;
  for (int i = 1;; i++) {
    const auto err = Sample(exp_guess);
    const auto derr = err.y2 - err.y1;
    exp_guess = NewtonEstimateX(exp_guess, err.y1, derr / d_exp);
    // Check if we were close before, because then this last round of estimation
    // should get us pretty much right on it.
    if (std::abs(err.y1) < error_tolerance) {
      return exp_guess;
    }
    if (i >= MAX_ITERS) {
      printf_stderr("GuessGamma() -> %f after %i iterations (avg err %f)\n",
                    exp_guess, i, err.y1);
      MOZ_ASSERT(false, "GuessGamma failed.");
      return exp_guess;
    }
  }
}

// -

ColorProfileDesc ColorProfileDesc::From(const qcms_profile& qcmsProfile) {
  ColorProfileDesc ret;

  qcms_profile_data data = {};
  qcms_profile_get_data(&qcmsProfile, &data);

  auto xyzd50FromLinearRgb = mat3{};
  // X contributions from [R,G,B]
  xyzd50FromLinearRgb.at(0, 0) = data.red_colorant_xyzd50[0];
  xyzd50FromLinearRgb.at(1, 0) = data.green_colorant_xyzd50[0];
  xyzd50FromLinearRgb.at(2, 0) = data.blue_colorant_xyzd50[0];
  // Y contributions from [R,G,B]
  xyzd50FromLinearRgb.at(0, 1) = data.red_colorant_xyzd50[1];
  xyzd50FromLinearRgb.at(1, 1) = data.green_colorant_xyzd50[1];
  xyzd50FromLinearRgb.at(2, 1) = data.blue_colorant_xyzd50[1];
  // Z contributions from [R,G,B]
  xyzd50FromLinearRgb.at(0, 2) = data.red_colorant_xyzd50[2];
  xyzd50FromLinearRgb.at(1, 2) = data.green_colorant_xyzd50[2];
  xyzd50FromLinearRgb.at(2, 2) = data.blue_colorant_xyzd50[2];

  const auto d65FromD50 = XyzAFromXyzB_BradfordLinear(D65, D50);
  ret.xyzd65FromLinearRgb = d65FromD50 * xyzd50FromLinearRgb;

  // -

  const auto Fn = [&](std::vector<float>* const linearFromTf,
                      int32_t claimed_samples,
                      const qcms_color_channel channel) {
    if (claimed_samples == 0) return;  // No tf.

    if (claimed_samples == -1) {
      claimed_samples = 4096;  // Ask it to generate a bunch.
      claimed_samples = 256;   // Ask it to generate a bunch.
    }

    linearFromTf->resize(AssertedCast<size_t>(claimed_samples));

    const auto begin = linearFromTf->data();
    qcms_profile_get_lut(&qcmsProfile, channel, begin,
                         begin + linearFromTf->size());
  };

  Fn(&ret.linearFromTf.r, data.linear_from_trc_red_samples,
     qcms_color_channel::Red);
  Fn(&ret.linearFromTf.b, data.linear_from_trc_blue_samples,
     qcms_color_channel::Blue);
  Fn(&ret.linearFromTf.g, data.linear_from_trc_green_samples,
     qcms_color_channel::Green);

  // -

  return ret;
}

// -

ColorProfileConversionDesc ColorProfileConversionDesc::From(
    const FromDesc& desc) {
  const auto dstLinearRgbFromXyzd65 = inverse(desc.dst.xyzd65FromLinearRgb);
  auto ret = ColorProfileConversionDesc{
      .srcRgbFromSrcYuv = desc.src.rgbFromYcbcr,
      .srcLinearFromSrcTf = desc.src.linearFromTf,
      .dstLinearFromSrcLinear =
          dstLinearRgbFromXyzd65 * desc.src.xyzd65FromLinearRgb,
      .dstTfFromDstLinear = {},
  };
  bool sameTF = true;
  sameTF &= desc.src.linearFromTf.r == desc.dst.linearFromTf.r;
  sameTF &= desc.src.linearFromTf.g == desc.dst.linearFromTf.g;
  sameTF &= desc.src.linearFromTf.b == desc.dst.linearFromTf.b;
  if (sameTF) {
    ret.srcLinearFromSrcTf = {};
    ret.dstTfFromDstLinear = {};
  } else {
    const auto Invert = [](const std::vector<float>& linearFromTf,
                           std::vector<float>* const tfFromLinear) {
      const auto size = linearFromTf.size();
      MOZ_ASSERT(size != 1);  // Less than two is uninvertable.
      if (size < 2) return;
      (*tfFromLinear).resize(size);
      InvertLut(linearFromTf, &*tfFromLinear);
    };
    Invert(desc.dst.linearFromTf.r, &ret.dstTfFromDstLinear.r);
    Invert(desc.dst.linearFromTf.g, &ret.dstTfFromDstLinear.g);
    Invert(desc.dst.linearFromTf.b, &ret.dstTfFromDstLinear.b);
  }
  return ret;
}

}  // namespace mozilla::color
