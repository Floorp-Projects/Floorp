/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGANIMATEDINTEGER_H_
#define DOM_SVG_DOMSVGANIMATEDINTEGER_H_

#include "nsWrapperCache.h"

#include "SVGElement.h"

namespace mozilla::dom {

class DOMSVGAnimatedInteger : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGAnimatedInteger)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGAnimatedInteger)

  SVGElement* GetParentObject() const { return mSVGElement; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  virtual int32_t BaseVal() = 0;
  virtual void SetBaseVal(int32_t aBaseVal) = 0;
  virtual int32_t AnimVal() = 0;

 protected:
  explicit DOMSVGAnimatedInteger(SVGElement* aSVGElement)
      : mSVGElement(aSVGElement) {}
  virtual ~DOMSVGAnimatedInteger() = default;

  RefPtr<SVGElement> mSVGElement;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_DOMSVGANIMATEDINTEGER_H_
