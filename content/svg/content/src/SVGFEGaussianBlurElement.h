/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEGaussianBlurElement_h
#define mozilla_dom_SVGFEGaussianBlurElement_h

#include "nsSVGFilters.h"

namespace mozilla {
namespace dom {

typedef nsSVGFE nsSVGFEGaussianBlurElementBase;

class nsSVGFEGaussianBlurElement : public nsSVGFEGaussianBlurElementBase,
                                   public nsIDOMSVGFEGaussianBlurElement
{
  friend nsresult NS_NewSVGFEGaussianBlurElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEGaussianBlurElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEGaussianBlurElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEGaussianBlurElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo >& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // Gaussian
  NS_DECL_NSIDOMSVGFEGAUSSIANBLURELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEGaussianBlurElementBase::)

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { STD_DEV };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

private:
  nsresult GetDXY(uint32_t *aDX, uint32_t *aDY, const nsSVGFilterInstance& aInstance);
  nsIntRect InflateRectForBlur(const nsIntRect& aRect, const nsSVGFilterInstance& aInstance);

  void GaussianBlur(const Image *aSource, const Image *aTarget,
                    const nsIntRect& aDataRect,
                    uint32_t aDX, uint32_t aDY);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEGaussianBlurElement_h
