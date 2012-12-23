/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGRAPHICELEMENT_H__
#define __NS_SVGGRAPHICELEMENT_H__

#include "gfxMatrix.h"
#include "nsIDOMSVGLocatable.h"
#include "nsIDOMSVGTransformable.h"
#include "nsSVGElement.h"
#include "SVGAnimatedTransformList.h"

typedef nsSVGElement nsSVGGraphicElementBase;

class nsSVGGraphicElement : public nsSVGGraphicElementBase,
                            public nsIDOMSVGTransformable // : nsIDOMSVGLocatable
{
protected:
  nsSVGGraphicElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGLOCATABLE
  NS_DECL_NSIDOMSVGTRANSFORMABLE

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const;

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
  // nsSVGElement overrides
  virtual bool IsEventName(nsIAtom* aName);

  nsAutoPtr<mozilla::SVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfxMatrix> mAnimateMotionTransform;
};

#endif // __NS_SVGGRAPHICELEMENT_H__
