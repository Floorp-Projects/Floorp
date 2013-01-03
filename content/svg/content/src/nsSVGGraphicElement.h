/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGRAPHICELEMENT_H__
#define __NS_SVGGRAPHICELEMENT_H__

#include "gfxMatrix.h"
#include "SVGTransformableElement.h"
#include "SVGAnimatedTransformList.h"
#include "DOMSVGTests.h"

typedef mozilla::dom::SVGTransformableElement nsSVGGraphicElementBase;

#define MOZILLA_SVGGRAPHICSELEMENT_IID \
  { 0xe57b8fe5, 0x9088, 0x446e, \
    {0xa1, 0x87, 0xd1, 0xdb, 0xbb, 0x58, 0xce, 0xdc}}

class nsSVGGraphicElement : public nsSVGGraphicElementBase,
                            public DOMSVGTests
{
protected:
  nsSVGGraphicElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_SVGGRAPHICSELEMENT_IID)
  NS_FORWARD_NSIDOMSVGLOCATABLE(mozilla::dom::SVGLocatableElement::)
  NS_FORWARD_NSIDOMSVGTRANSFORMABLE(mozilla::dom::SVGTransformableElement::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const;


  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;


  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;
  virtual const gfxMatrix* GetAnimateMotionTransform() const;
  virtual void SetAnimateMotionTransform(const gfxMatrix* aMatrix);

  virtual mozilla::SVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0);
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::transform;
  }

protected:
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap) MOZ_OVERRIDE;

  // nsSVGElement overrides

  nsAutoPtr<mozilla::SVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfxMatrix> mAnimateMotionTransform;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGGraphicElement,
                              MOZILLA_SVGGRAPHICSELEMENT_IID)

#endif // __NS_SVGGRAPHICELEMENT_H__
