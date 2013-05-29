/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGIRect_h
#define mozilla_dom_SVGIRect_h

#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/SVGRectBinding.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"
#include "nsIContent.h"

class nsSVGElement;

namespace mozilla {
namespace dom {

class SVGIRect : public nsISupports,
                 public nsWrapperCache
{
public:
  SVGIRect()
  {
    SetIsDOMBinding();
  }

  virtual ~SVGIRect()
  {
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
  {
    return SVGRectBinding::Wrap(aCx, aScope, this);
  }

  virtual nsIContent* GetParentObject() const = 0;

  virtual float X() const = 0;

  virtual void SetX(float aX, ErrorResult& aRv) = 0;

  virtual float Y() const = 0;

  virtual void SetY(float aY, ErrorResult& aRv) = 0;

  virtual float Width() const = 0;

  virtual void SetWidth(float aWidth, ErrorResult& aRv) = 0;

  virtual float Height() const = 0;

  virtual void SetHeight(float aHeight, ErrorResult& aRv) = 0;
};

} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_SVGIRect_h

