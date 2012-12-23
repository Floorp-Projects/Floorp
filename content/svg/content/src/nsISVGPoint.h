/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/SVGPointBinding.h"

class nsSVGElement;

// {d6b6c440-af8d-40ee-856b-02a317cab275}
#define MOZILLA_NSISVGPOINT_IID \
  { 0xd6b6c440, 0xaf8d, 0x40ee, \
    { 0x85, 0x6b, 0x02, 0xa3, 0x17, 0xca, 0xb2, 0x75 } }

namespace mozilla {

class DOMSVGMatrix;

/**
 * Class nsISVGPoint
 *
 * This class creates the DOM objects that wrap internal SVGPoint objects.
 * An nsISVGPoint can be either a DOMSVGPoint or a nsSVGTranslatePoint::DOMVal.
 */
class nsISVGPoint : public nsISupports,
                    public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_NSISVGPOINT_IID)

  /**
   * Generic ctor for DOMSVGPoint objects that are created for an attribute.
   */
  explicit nsISVGPoint()
  {
    SetIsDOMBinding();
  }

  // WebIDL
  virtual float X() = 0;
  virtual void SetX(float aX, ErrorResult& rv) = 0;
  virtual float Y() = 0;
  virtual void SetY(float aY, ErrorResult& rv) = 0;
  virtual already_AddRefed<nsISVGPoint> MatrixTransform(DOMSVGMatrix& matrix) = 0;
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
    { return dom::SVGPointBinding::Wrap(cx, scope, this, triedToWrap); }

  virtual nsISupports* GetParentObject() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsISVGPoint, MOZILLA_NSISVGPOINT_IID)

} // namespace mozilla

