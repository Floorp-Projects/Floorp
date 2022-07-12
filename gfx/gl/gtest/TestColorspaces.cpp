/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "Colorspaces.h"

namespace mozilla::color {
mat4 YuvFromYcbcr(const YcbcrDesc&);
float TfFromLinear(const PiecewiseGammaDesc&, float linear);
float LinearFromTf(const PiecewiseGammaDesc&, float tf);
}  // namespace mozilla::color

using namespace mozilla::color;

auto Calc8From8(const ColorspaceTransform& ct, const ivec3 in8) {
  const auto in = vec3(in8) / vec3(255);
  const auto out = ct.DstFromSrc(in);
  const auto out8 = ivec3(round(out * vec3(255)));
  return out8;
}

auto Sample8From8(const Lut3& lut, const vec3 in8) {
  const auto in = in8 / vec3(255);
  const auto out = lut.Sample(in);
  const auto out8 = ivec3(round(out * vec3(255)));
  return out8;
}

TEST(Colorspaces, YcbcrDesc_Narrow8)
{
  const auto m = YuvFromYcbcr(YcbcrDesc::Narrow8());

  const auto Yuv8 = [&](const ivec3 ycbcr8) {
      const auto ycbcr = vec4(vec3(ycbcr8) / 255, 1);
      const auto yuv = m * ycbcr;
      return ivec3(round(yuv * 255));
  };

  EXPECT_EQ(Yuv8({{ 16, 128, 128}}), (ivec3{{0, 0, 0}}));
  EXPECT_EQ(Yuv8({{ 17, 128, 128}}), (ivec3{{1, 0, 0}}));
  // y = 0.5 => (16 + 235) / 2 = 125.5
  EXPECT_EQ(Yuv8({{125, 128, 128}}), (ivec3{{127, 0, 0}}));
  EXPECT_EQ(Yuv8({{126, 128, 128}}), (ivec3{{128, 0, 0}}));
  EXPECT_EQ(Yuv8({{234, 128, 128}}), (ivec3{{254, 0, 0}}));
  EXPECT_EQ(Yuv8({{235, 128, 128}}), (ivec3{{255, 0, 0}}));

  // Check that we get the naive out-of-bounds behavior we'd expect:
  EXPECT_EQ(Yuv8({{ 15, 128, 128}}), (ivec3{{-1,0,0}}));
  EXPECT_EQ(Yuv8({{236, 128, 128}}), (ivec3{{256, 0, 0}}));
}

TEST(Colorspaces, YcbcrDesc_Full8)
{
  const auto m = YuvFromYcbcr(YcbcrDesc::Full8());

  const auto Yuv8 = [&](const ivec3 ycbcr8) {
      const auto ycbcr = vec4(vec3(ycbcr8) / 255, 1);
      const auto yuv = m * ycbcr;
      return ivec3(round(yuv * 255));
  };

  EXPECT_EQ(Yuv8({{ 0, 128, 128}}), (ivec3{{0, 0, 0}}));
  EXPECT_EQ(Yuv8({{ 1, 128, 128}}), (ivec3{{1, 0, 0}}));
  EXPECT_EQ(Yuv8({{127, 128, 128}}), (ivec3{{127, 0, 0}}));
  EXPECT_EQ(Yuv8({{128, 128, 128}}), (ivec3{{128, 0, 0}}));
  EXPECT_EQ(Yuv8({{254, 128, 128}}), (ivec3{{254, 0, 0}}));
  EXPECT_EQ(Yuv8({{255, 128, 128}}), (ivec3{{255, 0, 0}}));
}

TEST(Colorspaces, YcbcrDesc_Float)
{
  const auto m = YuvFromYcbcr(YcbcrDesc::Float());

  const auto Yuv8 = [&](const vec3 ycbcr8) {
      const auto ycbcr = vec4(vec3(ycbcr8) / 255, 1);
      const auto yuv = m * ycbcr;
      return ivec3(round(yuv * 255));
  };

  EXPECT_EQ(Yuv8({{ 0, 0.5*255, 0.5*255}}), (ivec3{{0, 0, 0}}));
  EXPECT_EQ(Yuv8({{ 1, 0.5*255, 0.5*255}}), (ivec3{{1, 0, 0}}));
  EXPECT_EQ(Yuv8({{127, 0.5*255, 0.5*255}}), (ivec3{{127, 0, 0}}));
  EXPECT_EQ(Yuv8({{128, 0.5*255, 0.5*255}}), (ivec3{{128, 0, 0}}));
  EXPECT_EQ(Yuv8({{254, 0.5*255, 0.5*255}}), (ivec3{{254, 0, 0}}));
  EXPECT_EQ(Yuv8({{255, 0.5*255, 0.5*255}}), (ivec3{{255, 0, 0}}));
}

TEST(Colorspaces, ColorspaceTransform_Rec709Narrow)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Narrow8()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {},
  };
  const auto ct = ColorspaceTransform::Create(src, dst);

  EXPECT_EQ(Calc8From8(ct, {{ 16, 128, 128}}), (ivec3{0}));
  EXPECT_EQ(Calc8From8(ct, {{ 17, 128, 128}}), (ivec3{1}));
  EXPECT_EQ(Calc8From8(ct, {{126, 128, 128}}), (ivec3{128}));
  EXPECT_EQ(Calc8From8(ct, {{234, 128, 128}}), (ivec3{254}));
  EXPECT_EQ(Calc8From8(ct, {{235, 128, 128}}), (ivec3{255}));

  // Check that we get the naive out-of-bounds behavior we'd expect:
  EXPECT_EQ(Calc8From8(ct, {{ 15, 128, 128}}), (ivec3{-1}));
  EXPECT_EQ(Calc8From8(ct, {{236, 128, 128}}), (ivec3{256}));
}

TEST(Colorspaces, LutSample_Rec709Float)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Float()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {},
  };
  const auto lut = ColorspaceTransform::Create(src, dst).ToLut3();

  EXPECT_EQ(Sample8From8(lut, {{  0, 0.5*255, 0.5*255}}), (ivec3{0}));
  EXPECT_EQ(Sample8From8(lut, {{  1, 0.5*255, 0.5*255}}), (ivec3{1}));
  EXPECT_EQ(Sample8From8(lut, {{127, 0.5*255, 0.5*255}}), (ivec3{127}));
  EXPECT_EQ(Sample8From8(lut, {{128, 0.5*255, 0.5*255}}), (ivec3{128}));
  EXPECT_EQ(Sample8From8(lut, {{254, 0.5*255, 0.5*255}}), (ivec3{254}));
  EXPECT_EQ(Sample8From8(lut, {{255, 0.5*255, 0.5*255}}), (ivec3{255}));
}

TEST(Colorspaces, LutSample_Rec709Narrow)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Narrow8()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {},
  };
  const auto lut = ColorspaceTransform::Create(src, dst).ToLut3();

  EXPECT_EQ(Sample8From8(lut, {{ 16, 128, 128}}), (ivec3{0}));
  EXPECT_EQ(Sample8From8(lut, {{ 17, 128, 128}}), (ivec3{1}));
  EXPECT_EQ(Sample8From8(lut, {{(235+16)/2, 128, 128}}), (ivec3{127}));
  EXPECT_EQ(Sample8From8(lut, {{(235+16)/2 + 1, 128, 128}}), (ivec3{128}));
  EXPECT_EQ(Sample8From8(lut, {{234, 128, 128}}), (ivec3{254}));
  EXPECT_EQ(Sample8From8(lut, {{235, 128, 128}}), (ivec3{255}));
}

TEST(Colorspaces, LutSample_Rec709Full)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Full8()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {},
  };
  const auto lut = ColorspaceTransform::Create(src, dst).ToLut3();

  EXPECT_EQ(Sample8From8(lut, {{  0, 128, 128}}), (ivec3{0}));
  EXPECT_EQ(Sample8From8(lut, {{  1, 128, 128}}), (ivec3{1}));
  EXPECT_EQ(Sample8From8(lut, {{ 16, 128, 128}}), (ivec3{16}));
  EXPECT_EQ(Sample8From8(lut, {{128, 128, 128}}), (ivec3{128}));
  EXPECT_EQ(Sample8From8(lut, {{235, 128, 128}}), (ivec3{235}));
  EXPECT_EQ(Sample8From8(lut, {{254, 128, 128}}), (ivec3{254}));
  EXPECT_EQ(Sample8From8(lut, {{255, 128, 128}}), (ivec3{255}));
}

TEST(Colorspaces, PiecewiseGammaDesc_Srgb)
{
  const auto tf = PiecewiseGammaDesc::Srgb();

  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x00 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x01 / 255.0) * 255 )), 0x0d);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x37 / 255.0) * 255 )), 0x80);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x80 / 255.0) * 255 )), 0xbc);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0xfd / 255.0) * 255 )), 0xfe);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0xfe / 255.0) * 255 )), 0xff);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0xff / 255.0) * 255 )), 0xff);

  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x00 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x01 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x06 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x07 / 255.0) * 255 )), 0x01);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x0d / 255.0) * 255 )), 0x01);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x80 / 255.0) * 255 )), 0x37);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0xbc / 255.0) * 255 )), 0x80);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0xfe / 255.0) * 255 )), 0xfd);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0xff / 255.0) * 255 )), 0xff);
}

TEST(Colorspaces, PiecewiseGammaDesc_Rec709)
{
  const auto tf = PiecewiseGammaDesc::Rec709();

  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x00 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x01 / 255.0) * 255 )), 0x05);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x43 / 255.0) * 255 )), 0x80);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0x80 / 255.0) * 255 )), 0xb4);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0xfd / 255.0) * 255 )), 0xfe);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0xfe / 255.0) * 255 )), 0xff);
  EXPECT_EQ(int(roundf( TfFromLinear(tf, 0xff / 255.0) * 255 )), 0xff);

  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x00 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x01 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x02 / 255.0) * 255 )), 0x00);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x03 / 255.0) * 255 )), 0x01);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x05 / 255.0) * 255 )), 0x01);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0x80 / 255.0) * 255 )), 0x43);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0xb4 / 255.0) * 255 )), 0x80);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0xfe / 255.0) * 255 )), 0xfd);
  EXPECT_EQ(int(roundf( LinearFromTf(tf, 0xff / 255.0) * 255 )), 0xff);
}

TEST(Colorspaces, ColorspaceTransform_PiecewiseGammaDesc)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Srgb(),
    {},
    {},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Srgb(),
    PiecewiseGammaDesc::Srgb(),
    {},
  };
  const auto toGamma = ColorspaceTransform::Create(src, dst);
  const auto toLinear = ColorspaceTransform::Create(dst, src);

  EXPECT_EQ(Calc8From8(toGamma, ivec3{0x00}), (ivec3{0x00}));
  EXPECT_EQ(Calc8From8(toGamma, ivec3{0x01}), (ivec3{0x0d}));
  EXPECT_EQ(Calc8From8(toGamma, ivec3{0x37}), (ivec3{0x80}));
  EXPECT_EQ(Calc8From8(toGamma, ivec3{0x80}), (ivec3{0xbc}));
  EXPECT_EQ(Calc8From8(toGamma, ivec3{0xfd}), (ivec3{0xfe}));
  EXPECT_EQ(Calc8From8(toGamma, ivec3{0xff}), (ivec3{0xff}));

  EXPECT_EQ(Calc8From8(toLinear, ivec3{0x00}), (ivec3{0x00}));
  EXPECT_EQ(Calc8From8(toLinear, ivec3{0x0d}), (ivec3{0x01}));
  EXPECT_EQ(Calc8From8(toLinear, ivec3{0x80}), (ivec3{0x37}));
  EXPECT_EQ(Calc8From8(toLinear, ivec3{0xbc}), (ivec3{0x80}));
  EXPECT_EQ(Calc8From8(toLinear, ivec3{0xfe}), (ivec3{0xfd}));
  EXPECT_EQ(Calc8From8(toLinear, ivec3{0xff}), (ivec3{0xff}));
}

// -
// Actual end-to-end tests

TEST(Colorspaces, SrgbFromRec709)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Narrow8()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Srgb(),
    PiecewiseGammaDesc::Srgb(),
    {},
  };
  const auto ct = ColorspaceTransform::Create(src, dst);

  EXPECT_EQ(Calc8From8(ct, ivec3{{16, 128, 128}}), (ivec3{0}));
  EXPECT_EQ(Calc8From8(ct, ivec3{{17, 128, 128}}), (ivec3{3}));
  EXPECT_EQ(Calc8From8(ct, ivec3{{115, 128, 128}}), (ivec3{128}));
  EXPECT_EQ(Calc8From8(ct, ivec3{{126, 128, 128}}), (ivec3{140}));
  EXPECT_EQ(Calc8From8(ct, ivec3{{234, 128, 128}}), (ivec3{254}));
  EXPECT_EQ(Calc8From8(ct, ivec3{{235, 128, 128}}), (ivec3{255}));
}

TEST(Colorspaces, SrgbFromDisplayP3)
{
  const auto p3C = ColorspaceDesc{
    Chromaticities::DisplayP3(),
    PiecewiseGammaDesc::DisplayP3(),
  };
  const auto srgbC = ColorspaceDesc{
    Chromaticities::Srgb(),
    PiecewiseGammaDesc::Srgb(),
  };
  const auto srgbLinearC = ColorspaceDesc{
    Chromaticities::Srgb(),
    {},
  };
  const auto srgbFromP3 = ColorspaceTransform::Create(p3C, srgbC);
  const auto srgbLinearFromP3 = ColorspaceTransform::Create(p3C, srgbLinearC);

  // E.g. https://colorjs.io/apps/convert/?color=color(display-p3%200.4%200.8%200.4)&precision=4
  auto srgb = srgbFromP3.DstFromSrc(vec3{{0.4, 0.8, 0.4}});
  EXPECT_NEAR(srgb.x(), 0.179, 0.001);
  EXPECT_NEAR(srgb.y(), 0.812, 0.001);
  EXPECT_NEAR(srgb.z(), 0.342, 0.001);
  auto srgbLinear = srgbLinearFromP3.DstFromSrc(vec3{{0.4, 0.8, 0.4}});
  EXPECT_NEAR(srgbLinear.x(), 0.027, 0.001);
  EXPECT_NEAR(srgbLinear.y(), 0.624, 0.001);
  EXPECT_NEAR(srgbLinear.z(), 0.096, 0.001);
}

// -

struct Stats {
  double mean = 0;
  double variance = 0;
  double min = 1.0/0;
  double max = -1.0/0;

  template<class T>
  static Stats For(const T& iterable) {
    auto ret = Stats{};
    for (const auto& cur : iterable) {
      ret.mean += cur;
      ret.min = std::min(ret.min, cur);
      ret.max = std::max(ret.max, cur);
    }
    ret.mean /= iterable.size();
    for (const auto& cur : iterable) {
      ret.variance += pow(cur - ret.mean, 2);
    }
    ret.variance /= iterable.size();
    return ret;
  }

  constexpr double standardDeviation() const {
    return sqrt(variance);
  }
};

static Stats StatsForLutError(const ColorspaceTransform& ct,
    const ivec3 srcQuants,
    const ivec3 dstQuants) {
  const auto lut = ct.ToLut3();

  const auto dstScale = vec3(dstQuants-1);

  std::vector<double> quantErrors;
  quantErrors.reserve(srcQuants.x() * srcQuants.y() * srcQuants.z());
  ForEachSampleWithin(srcQuants, [&](const vec3& src) {
    const auto sampled = lut.Sample(src);
    const auto actual = ct.DstFromSrc(src);
    const auto isampled = ivec3(round(sampled * dstScale));
    const auto iactual = ivec3(round(actual * dstScale));
    const auto ierr = abs(isampled - iactual);
    const auto quantError = dot(ierr, ivec3{1});
    quantErrors.push_back(quantError);
    if (quantErrors.size() % 100000 == 0) {
      printf("%zu of %zu\n", quantErrors.size(), quantErrors.capacity());
    }
  });

  const auto quantErrStats = Stats::For(quantErrors);
  return quantErrStats;
}

TEST(Colorspaces, LutError_Rec709Full_Rec709Rgb)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Full8()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {},
  };
  const auto ct = ColorspaceTransform::Create(src, dst);
  const auto stats = StatsForLutError(ct, ivec3{64}, ivec3{256});
  EXPECT_NEAR(stats.mean, 0.000, 0.001);
  EXPECT_NEAR(stats.standardDeviation(), 0.008, 0.001);
  EXPECT_NEAR(stats.min, 0, 0.001);
  EXPECT_NEAR(stats.max, 1, 0.001);
}

TEST(Colorspaces, LutError_Rec709Full_Srgb)
{
  const auto src = ColorspaceDesc{
    Chromaticities::Rec709(),
    PiecewiseGammaDesc::Rec709(),
    {{YuvLumaCoeffs::Rec709(), YcbcrDesc::Full8()}},
  };
  const auto dst = ColorspaceDesc{
    Chromaticities::Srgb(),
    PiecewiseGammaDesc::Srgb(),
    {},
  };
  const auto ct = ColorspaceTransform::Create(src, dst);
  const auto stats = StatsForLutError(ct, ivec3{64}, ivec3{256});
  EXPECT_NEAR(stats.mean, 0.530, 0.001);
  EXPECT_NEAR(stats.standardDeviation(), 1.674, 0.001);
  EXPECT_NEAR(stats.min, 0, 0.001);
  EXPECT_NEAR(stats.max, 17, 0.001);
}
