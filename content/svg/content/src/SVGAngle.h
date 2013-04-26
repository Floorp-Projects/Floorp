/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsWrapperCache.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"

class nsSVGAngle;

// We make SVGAngle a pseudo-interface to allow us to QI to it in order
// to check that the objects that scripts pass in are our our *native*
// transform objects.

// {da9670f6-6d3d-4fb3-974c-9d6bad8dcd53}
#define MOZILLA_SVGANGLE_IID \
{0x2cd27ef5, 0x81d8, 0x4720, \
  {0x81, 0x42, 0x66, 0xc6, 0xa9, 0xbe, 0xc3, 0xeb } }


namespace mozilla {
namespace dom {

class SVGAngle MOZ_FINAL : public nsISupports,
                           public nsWrapperCache
{
public:
  typedef enum {
    BaseValue,
    AnimValue,
    CreatedValue
  } AngleType;

  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_SVGANGLE_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SVGAngle)

  SVGAngle(nsSVGAngle* aVal, nsSVGElement *aSVGElement, AngleType aType)
    : mVal(aVal), mSVGElement(aSVGElement), mType(aType)
  {
    SetIsDOMBinding();
  }

  ~SVGAngle();

  // WebIDL
  nsSVGElement* GetParentObject() { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
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
  nsSVGAngle* mVal; // if mType is CreatedValue, we own the angle.  Otherwise, the element does.
  nsRefPtr<nsSVGElement> mSVGElement;
  AngleType mType;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGAngle, MOZILLA_SVGANGLE_IID)

} //namespace dom
} //namespace mozilla

