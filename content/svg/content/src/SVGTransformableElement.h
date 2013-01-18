/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SVGTransformableElement_h
#define SVGTransformableElement_h

#include "mozilla/dom/SVGLocatableElement.h"
#include "gfxMatrix.h"
#include "SVGAnimatedTransformList.h"

#define MOZILLA_SVGTRANSFORMABLEELEMENT_IID \
  { 0x77888cba, 0x0b43, 0x4654, \
    {0x96, 0x3c, 0xf5, 0x50, 0xfc, 0xb5, 0x5e, 0x32}}

namespace mozilla {
class DOMSVGAnimatedTransformList;

namespace dom {
class SVGTransformableElement : public SVGLocatableElement
{
public:
  SVGTransformableElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGLocatableElement(aNodeInfo) {}
  virtual ~SVGTransformableElement() {}

  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_SVGTRANSFORMABLEELEMENT_IID)
  NS_DECL_ISUPPORTS_INHERITED

  // WebIDL
  already_AddRefed<DOMSVGAnimatedTransformList> Transform();

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const;


  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;


  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;
  virtual const gfxMatrix* GetAnimateMotionTransform() const;
  virtual void SetAnimateMotionTransform(const gfxMatrix* aMatrix);

  virtual SVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0);
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::transform;
  }

protected:
  // nsSVGElement overrides

  nsAutoPtr<SVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfxMatrix> mAnimateMotionTransform;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGTransformableElement,
                              MOZILLA_SVGTRANSFORMABLEELEMENT_IID)

} // namespace dom
} // namespace mozilla

#endif // SVGTransformableElement_h

