/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFETurbulenceElement_h
#define mozilla_dom_SVGFETurbulenceElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"
#include "nsSVGInteger.h"
#include "nsSVGString.h"

nsresult NS_NewSVGFETurbulenceElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFETurbulenceElementBase;

class SVGFETurbulenceElement : public SVGFETurbulenceElementBase
{
  friend nsresult (::NS_NewSVGFETurbulenceElement(nsIContent **aResult,
                                                  already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFETurbulenceElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFETurbulenceElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  virtual bool SubregionIsUnionOfRegions() { return false; }

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedNumber> BaseFrequencyX();
  already_AddRefed<nsIDOMSVGAnimatedNumber> BaseFrequencyY();
  already_AddRefed<nsIDOMSVGAnimatedInteger> NumOctaves();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Seed();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> StitchTiles();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Type();

protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { SEED }; // floating point seed?!
  nsSVGNumber2 mNumberAttributes[1];
  static NumberInfo sNumberInfo[1];

  enum { BASE_FREQ };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { OCTAVES };
  nsSVGInteger mIntegerAttributes[1];
  static IntegerInfo sIntegerInfo[1];

  enum { TYPE, STITCHTILES };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sTypeMap[];
  static nsSVGEnumMapping sStitchTilesMap[];
  static EnumInfo sEnumInfo[2];

  enum { RESULT };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

private:

  /* The turbulence calculation code is an adapted version of what
     appears in the SVG 1.1 specification:
         http://www.w3.org/TR/SVG11/filters.html#feTurbulence
  */

  /* Produces results in the range [1, 2**31 - 2].
     Algorithm is: r = (a * r) mod m
     where a = 16807 and m = 2**31 - 1 = 2147483647
     See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988
     To test: the algorithm should produce the result 1043618065
     as the 10,000th generated number if the original seed is 1.
  */
#define RAND_M 2147483647	/* 2**31 - 1 */
#define RAND_A 16807		/* 7**5; primitive root of m */
#define RAND_Q 127773		/* m / a */
#define RAND_R 2836		/* m % a */

  int32_t SetupSeed(int32_t aSeed) {
    if (aSeed <= 0)
      aSeed = -(aSeed % (RAND_M - 1)) + 1;
    if (aSeed > RAND_M - 1)
      aSeed = RAND_M - 1;
    return aSeed;
  }

  uint32_t Random(uint32_t aSeed) {
    int32_t result = RAND_A * (aSeed % RAND_Q) - RAND_R * (aSeed / RAND_Q);
    if (result <= 0)
      result += RAND_M;
    return result;
  }
#undef RAND_M
#undef RAND_A
#undef RAND_Q
#undef RAND_R

  const static int sBSize = 0x100;
  const static int sBM = 0xff;
  const static int sPerlinN = 0x1000;
  const static int sNP = 12;			/* 2^PerlinN */
  const static int sNM = 0xfff;

  int32_t mLatticeSelector[sBSize + sBSize + 2];
  double mGradient[4][sBSize + sBSize + 2][2];
  struct StitchInfo {
    int mWidth;			// How much to subtract to wrap for stitching.
    int mHeight;
    int mWrapX;			// Minimum value to wrap.
    int mWrapY;
  };

  void InitSeed(int32_t aSeed);
  double Noise2(int aColorChannel, double aVec[2], StitchInfo *aStitchInfo);
  double
  Turbulence(int aColorChannel, double *aPoint, double aBaseFreqX,
             double aBaseFreqY, int aNumOctaves, bool aFractalSum,
             bool aDoStitching, double aTileX, double aTileY,
             double aTileWidth, double aTileHeight);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFETurbulenceElement_h
