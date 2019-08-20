/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGRect_h
#define mozilla_dom_SVGRect_h

#include "mozilla/dom/SVGElement.h"
#include "mozilla/gfx/Rect.h"

////////////////////////////////////////////////////////////////////////
// SVGRect class

namespace mozilla {
namespace dom {

class SVGSVGElement;

class SVGRect final : public nsWrapperCache {
 public:
  typedef enum { BaseValue, AnimValue, CreatedValue } RectType;

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SVGRect)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(SVGRect)

  /**
   * Generic ctor for objects that are created for an attribute.
   */
  SVGRect(SVGAnimatedViewBox* aVal, SVGElement* aSVGElement, RectType aType)
      : mVal(aVal), mParent(aSVGElement), mType(aType) {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mType == BaseValue || mType == AnimValue);
  }

  /**
   * Ctor for creating the objects returned by SVGSVGElement.createSVGRect(),
   * which do not initially belong to an attribute.
   */
  explicit SVGRect(SVGSVGElement* aSVGElement);

  /**
   * Ctor for all other non-attribute usage i.e getBBox, getExtentOfChar etc.
   */
  SVGRect(nsIContent* aParent, const gfx::Rect& aRect)
      : mVal(nullptr), mRect(aRect), mParent(aParent), mType(CreatedValue) {
    MOZ_ASSERT(mParent);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  float X();
  float Y();
  float Width();
  float Height();

  void SetX(float aX, mozilla::ErrorResult& aRv);
  void SetY(float aY, mozilla::ErrorResult& aRv);
  void SetWidth(float aWidth, mozilla::ErrorResult& aRv);
  void SetHeight(float aHeight, mozilla::ErrorResult& aRv);

  nsIContent* GetParentObject() const {
    MOZ_ASSERT(mParent);
    return mParent;
  }

 private:
  virtual ~SVGRect();

  // If we're actually representing a viewBox rect then our value
  // will come from that element's viewBox attribute's value.
  SVGAnimatedViewBox* mVal;  // kept alive because it belongs to content
  gfx::Rect mRect;

  // If mType is AnimValue or BaseValue this will be an element that
  // has a viewBox, otherwise it could be any nsIContent.
  RefPtr<nsIContent> mParent;
  const RectType mType;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGRect_h
