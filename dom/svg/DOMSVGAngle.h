/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGANGLE_H_
#define DOM_SVG_DOMSVGANGLE_H_

#include "nsWrapperCache.h"
#include "SVGElement.h"
#include "mozilla/Attributes.h"

namespace mozilla {

class SVGAnimatedOrient;

namespace dom {
class SVGSVGElement;

class DOMSVGAngle final : public nsWrapperCache {
 public:
  enum class AngleType : int8_t { BaseValue, AnimValue, CreatedValue };

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGAngle)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGAngle)

  /**
   * Generic ctor for DOMSVGAngle objects that are created for an attribute.
   */
  DOMSVGAngle(SVGAnimatedOrient* aVal, SVGElement* aSVGElement, AngleType aType)
      : mVal(aVal), mSVGElement(aSVGElement), mType(aType) {}

  /**
   * Ctor for creating the objects returned by SVGSVGElement.createSVGAngle(),
   * which do not initially belong to an attribute.
   */
  explicit DOMSVGAngle(SVGSVGElement* aSVGElement);

  // WebIDL
  SVGElement* GetParentObject() { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  uint16_t UnitType() const;
  float Value() const;
  void GetValueAsString(nsAString& aValue);
  void SetValue(float aValue, ErrorResult& rv);
  float ValueInSpecifiedUnits() const;
  void SetValueInSpecifiedUnits(float aValue, ErrorResult& rv);
  void SetValueAsString(const nsAString& aValue, ErrorResult& rv);
  void NewValueSpecifiedUnits(uint16_t unitType, float value, ErrorResult& rv);
  void ConvertToSpecifiedUnits(uint16_t unitType, ErrorResult& rv);

 protected:
  ~DOMSVGAngle();

  SVGAnimatedOrient* mVal;  // if mType is CreatedValue, we own the angle.
                            // Otherwise, the element does.
  RefPtr<SVGElement> mSVGElement;
  AngleType mType;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_DOMSVGANGLE_H_
