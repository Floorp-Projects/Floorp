/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimatedRect_h
#define mozilla_dom_SVGAnimatedRect_h

#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGRectBinding.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"

namespace mozilla {
class SVGAnimatedViewBox;
namespace dom {

class SVGAnimatedRect final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SVGAnimatedRect)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(SVGAnimatedRect)

  SVGAnimatedRect(SVGAnimatedViewBox* aVal, SVGElement* aSVGElement);

  SVGElement* GetParentObject() const { return mSVGElement; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<SVGIRect> GetBaseVal();

  already_AddRefed<SVGIRect> GetAnimVal();

 private:
  virtual ~SVGAnimatedRect();

  SVGAnimatedViewBox* mVal;  // kept alive because it belongs to content
  RefPtr<SVGElement> mSVGElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGAnimatedRect_h
