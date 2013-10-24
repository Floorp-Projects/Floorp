/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFETurbulenceElement.h"
#include "mozilla/dom/SVGFETurbulenceElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FETurbulence)

namespace mozilla {
namespace dom {

// Turbulence Types
static const unsigned short SVG_TURBULENCE_TYPE_FRACTALNOISE = 1;
static const unsigned short SVG_TURBULENCE_TYPE_TURBULENCE = 2;

// Stitch Options
static const unsigned short SVG_STITCHTYPE_STITCH = 1;
static const unsigned short SVG_STITCHTYPE_NOSTITCH = 2;

static const int32_t MAX_OCTAVES = 10;

JSObject*
SVGFETurbulenceElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFETurbulenceElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGFETurbulenceElement::sNumberInfo[1] =
{
  { &nsGkAtoms::seed, 0, false }
};

nsSVGElement::NumberPairInfo SVGFETurbulenceElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::baseFrequency, 0, 0 }
};

nsSVGElement::IntegerInfo SVGFETurbulenceElement::sIntegerInfo[1] =
{
  { &nsGkAtoms::numOctaves, 1 }
};

nsSVGEnumMapping SVGFETurbulenceElement::sTypeMap[] = {
  {&nsGkAtoms::fractalNoise,
   SVG_TURBULENCE_TYPE_FRACTALNOISE},
  {&nsGkAtoms::turbulence,
   SVG_TURBULENCE_TYPE_TURBULENCE},
  {nullptr, 0}
};

nsSVGEnumMapping SVGFETurbulenceElement::sStitchTilesMap[] = {
  {&nsGkAtoms::stitch,
   SVG_STITCHTYPE_STITCH},
  {&nsGkAtoms::noStitch,
   SVG_STITCHTYPE_NOSTITCH},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFETurbulenceElement::sEnumInfo[2] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    SVG_TURBULENCE_TYPE_TURBULENCE
  },
  { &nsGkAtoms::stitchTiles,
    sStitchTilesMap,
    SVG_STITCHTYPE_NOSTITCH
  }
};

nsSVGElement::StringInfo SVGFETurbulenceElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFETurbulenceElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedNumber>
SVGFETurbulenceElement::BaseFrequencyX()
{
  return mNumberPairAttributes[BASE_FREQ].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst, this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFETurbulenceElement::BaseFrequencyY()
{
  return mNumberPairAttributes[BASE_FREQ].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond, this);
}

already_AddRefed<SVGAnimatedInteger>
SVGFETurbulenceElement::NumOctaves()
{
  return mIntegerAttributes[OCTAVES].ToDOMAnimatedInteger(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFETurbulenceElement::Seed()
{
  return mNumberAttributes[SEED].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFETurbulenceElement::StitchTiles()
{
  return mEnumAttributes[STITCHTILES].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFETurbulenceElement::Type()
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

nsresult
SVGFETurbulenceElement::Filter(nsSVGFilterInstance* instance,
                               const nsTArray<const Image*>& aSources,
                               const Image* aTarget,
                               const nsIntRect& rect)
{
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  nsIntRect filterSubregion(int32_t(aTarget->mFilterPrimitiveSubregion.X()),
                            int32_t(aTarget->mFilterPrimitiveSubregion.Y()),
                            int32_t(aTarget->mFilterPrimitiveSubregion.Width()),
                            int32_t(aTarget->mFilterPrimitiveSubregion.Height()));

  float fX = mNumberPairAttributes[BASE_FREQ].GetAnimValue(nsSVGNumberPair::eFirst);
  float fY = mNumberPairAttributes[BASE_FREQ].GetAnimValue(nsSVGNumberPair::eSecond);
  float seed = mNumberAttributes[OCTAVES].GetAnimValue();
  int32_t octaves = std::min(mIntegerAttributes[OCTAVES].GetAnimValue(), MAX_OCTAVES);
  uint16_t type = mEnumAttributes[TYPE].GetAnimValue();
  uint16_t stitch = mEnumAttributes[STITCHTILES].GetAnimValue();

  InitSeed((int32_t)seed);

  // XXXroc this makes absolutely no sense to me.
  float filterX = instance->GetFilterRegion().X();
  float filterY = instance->GetFilterRegion().Y();
  float filterWidth = instance->GetFilterRegion().Width();
  float filterHeight = instance->GetFilterRegion().Height();

  bool doStitch = false;
  if (stitch == SVG_STITCHTYPE_STITCH) {
    doStitch = true;

    float lowFreq, hiFreq;

    lowFreq = floor(filterWidth * fX) / filterWidth;
    hiFreq = ceil(filterWidth * fX) / filterWidth;
    if (fX / lowFreq < hiFreq / fX)
      fX = lowFreq;
    else
      fX = hiFreq;

    lowFreq = floor(filterHeight * fY) / filterHeight;
    hiFreq = ceil(filterHeight * fY) / filterHeight;
    if (fY / lowFreq < hiFreq / fY)
      fY = lowFreq;
    else
      fY = hiFreq;
  }
  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      int32_t targIndex = y * stride + x * 4;
      double point[2];
      point[0] = filterX + (filterWidth * (x + instance->GetSurfaceRect().x)) / (filterSubregion.width - 1);
      point[1] = filterY + (filterHeight * (y + instance->GetSurfaceRect().y)) / (filterSubregion.height - 1);

      float col[4];
      if (type == SVG_TURBULENCE_TYPE_TURBULENCE) {
        for (int i = 0; i < 4; i++)
          col[i] = Turbulence(i, point, fX, fY, octaves, false,
                              doStitch, filterX, filterY, filterWidth, filterHeight) * 255;
      } else {
        for (int i = 0; i < 4; i++)
          col[i] = (Turbulence(i, point, fX, fY, octaves, true,
                               doStitch, filterX, filterY, filterWidth, filterHeight) * 255 + 255) / 2;
      }
      for (int i = 0; i < 4; i++) {
        col[i] = std::min(col[i], 255.f);
        col[i] = std::max(col[i], 0.f);
      }

      uint8_t r, g, b, a;
      a = uint8_t(col[3]);
      FAST_DIVIDE_BY_255(r, unsigned(col[0]) * a);
      FAST_DIVIDE_BY_255(g, unsigned(col[1]) * a);
      FAST_DIVIDE_BY_255(b, unsigned(col[2]) * a);

      targetData[targIndex + GFX_ARGB32_OFFSET_B] = b;
      targetData[targIndex + GFX_ARGB32_OFFSET_G] = g;
      targetData[targIndex + GFX_ARGB32_OFFSET_R] = r;
      targetData[targIndex + GFX_ARGB32_OFFSET_A] = a;
    }
  }

  return NS_OK;
}

bool
SVGFETurbulenceElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                    nsIAtom* aAttribute) const
{
  return SVGFETurbulenceElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::seed ||
           aAttribute == nsGkAtoms::baseFrequency ||
           aAttribute == nsGkAtoms::numOctaves ||
           aAttribute == nsGkAtoms::type ||
           aAttribute == nsGkAtoms::stitchTiles));
}

void
SVGFETurbulenceElement::InitSeed(int32_t aSeed)
{
  double s;
  int i, j, k;
  aSeed = SetupSeed(aSeed);
  for (k = 0; k < 4; k++) {
    for (i = 0; i < sBSize; i++) {
      mLatticeSelector[i] = i;
      for (j = 0; j < 2; j++) {
        mGradient[k][i][j] =
          (double) (((aSeed =
                      Random(aSeed)) % (sBSize + sBSize)) - sBSize) / sBSize;
      }
      s = double (sqrt
                  (mGradient[k][i][0] * mGradient[k][i][0] +
                   mGradient[k][i][1] * mGradient[k][i][1]));
      mGradient[k][i][0] /= s;
      mGradient[k][i][1] /= s;
    }
  }
  while (--i) {
    k = mLatticeSelector[i];
    mLatticeSelector[i] = mLatticeSelector[j =
                                           (aSeed =
                                            Random(aSeed)) % sBSize];
    mLatticeSelector[j] = k;
  }
  for (i = 0; i < sBSize + 2; i++) {
    mLatticeSelector[sBSize + i] = mLatticeSelector[i];
    for (k = 0; k < 4; k++)
      for (j = 0; j < 2; j++)
        mGradient[k][sBSize + i][j] = mGradient[k][i][j];
  }
}

#define S_CURVE(t) ( t * t * (3. - 2. * t) )
#define LERP(t, a, b) ( a + t * (b - a) )
double
SVGFETurbulenceElement::Noise2(int aColorChannel, double aVec[2],
                               StitchInfo *aStitchInfo)
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  double rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
  long i, j;
  t = aVec[0] + sPerlinN;
  bx0 = (int) t;
  bx1 = bx0 + 1;
  rx0 = t - (int) t;
  rx1 = rx0 - 1.0f;
  t = aVec[1] + sPerlinN;
  by0 = (int) t;
  by1 = by0 + 1;
  ry0 = t - (int) t;
  ry1 = ry0 - 1.0f;
  // If stitching, adjust lattice points accordingly.
  if (aStitchInfo != nullptr) {
    if (bx0 >= aStitchInfo->mWrapX)
      bx0 -= aStitchInfo->mWidth;
    if (bx1 >= aStitchInfo->mWrapX)
      bx1 -= aStitchInfo->mWidth;
    if (by0 >= aStitchInfo->mWrapY)
      by0 -= aStitchInfo->mHeight;
    if (by1 >= aStitchInfo->mWrapY)
      by1 -= aStitchInfo->mHeight;
  }
  bx0 &= sBM;
  bx1 &= sBM;
  by0 &= sBM;
  by1 &= sBM;
  i = mLatticeSelector[bx0];
  j = mLatticeSelector[bx1];
  b00 = mLatticeSelector[i + by0];
  b10 = mLatticeSelector[j + by0];
  b01 = mLatticeSelector[i + by1];
  b11 = mLatticeSelector[j + by1];
  sx = double (S_CURVE(rx0));
  sy = double (S_CURVE(ry0));
  q = mGradient[aColorChannel][b00];
  u = rx0 * q[0] + ry0 * q[1];
  q = mGradient[aColorChannel][b10];
  v = rx1 * q[0] + ry0 * q[1];
  a = LERP(sx, u, v);
  q = mGradient[aColorChannel][b01];
  u = rx0 * q[0] + ry1 * q[1];
  q = mGradient[aColorChannel][b11];
  v = rx1 * q[0] + ry1 * q[1];
  b = LERP(sx, u, v);
  return LERP(sy, a, b);
}
#undef S_CURVE
#undef LERP

double
SVGFETurbulenceElement::Turbulence(int aColorChannel, double* aPoint,
                                   double aBaseFreqX, double aBaseFreqY,
                                   int aNumOctaves, bool aFractalSum,
                                   bool aDoStitching,
                                   double aTileX, double aTileY,
                                   double aTileWidth, double aTileHeight)
{
  StitchInfo stitch;
  StitchInfo *stitchInfo = nullptr; // Not stitching when nullptr.
  // Adjust the base frequencies if necessary for stitching.
  if (aDoStitching) {
    // When stitching tiled turbulence, the frequencies must be adjusted
    // so that the tile borders will be continuous.
    if (aBaseFreqX != 0.0) {
      double loFreq = double (floor(aTileWidth * aBaseFreqX)) / aTileWidth;
      double hiFreq = double (ceil(aTileWidth * aBaseFreqX)) / aTileWidth;
      if (aBaseFreqX / loFreq < hiFreq / aBaseFreqX)
        aBaseFreqX = loFreq;
      else
        aBaseFreqX = hiFreq;
    }
    if (aBaseFreqY != 0.0) {
      double loFreq = double (floor(aTileHeight * aBaseFreqY)) / aTileHeight;
      double hiFreq = double (ceil(aTileHeight * aBaseFreqY)) / aTileHeight;
      if (aBaseFreqY / loFreq < hiFreq / aBaseFreqY)
        aBaseFreqY = loFreq;
      else
        aBaseFreqY = hiFreq;
    }
    // Set up initial stitch values.
    stitchInfo = &stitch;
    stitch.mWidth = int (aTileWidth * aBaseFreqX + 0.5f);
    stitch.mWrapX = int (aTileX * aBaseFreqX + sPerlinN + stitch.mWidth);
    stitch.mHeight = int (aTileHeight * aBaseFreqY + 0.5f);
    stitch.mWrapY = int (aTileY * aBaseFreqY + sPerlinN + stitch.mHeight);
  }
  double sum = 0.0f;
  double vec[2];
  vec[0] = aPoint[0] * aBaseFreqX;
  vec[1] = aPoint[1] * aBaseFreqY;
  double ratio = 1;
  for (int octave = 0; octave < aNumOctaves; octave++) {
    if (aFractalSum)
      sum += double (Noise2(aColorChannel, vec, stitchInfo) / ratio);
    else
      sum += double (fabs(Noise2(aColorChannel, vec, stitchInfo)) / ratio);
    vec[0] *= 2;
    vec[1] *= 2;
    ratio *= 2;
    if (stitchInfo != nullptr) {
      // Update stitch values. Subtracting sPerlinN before the multiplication
      // and adding it afterward simplifies to subtracting it once.
      stitch.mWidth *= 2;
      stitch.mWrapX = 2 * stitch.mWrapX - sPerlinN;
      stitch.mHeight *= 2;
      stitch.mWrapY = 2 * stitch.mWrapY - sPerlinN;
    }
  }
  return sum;
}

nsIntRect
SVGFETurbulenceElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFETurbulenceElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
SVGFETurbulenceElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                 ArrayLength(sNumberPairInfo));
}

nsSVGElement::IntegerAttributesInfo
SVGFETurbulenceElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               ArrayLength(sIntegerInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFETurbulenceElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFETurbulenceElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
