/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGLength.h"

#include "DOMSVGLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "nsError.h"
#include "nsMathUtils.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAttrTearoffTable.h"
#include "SVGLength.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/FloatingPoint.h"

// See the architecture comment in DOMSVGAnimatedLengthList.h.

namespace mozilla {

namespace dom {

static SVGAttrTearoffTable<SVGAnimatedLength, DOMSVGLength>
    sBaseSVGLengthTearOffTable, sAnimSVGLengthTearOffTable;

// We could use NS_IMPL_CYCLE_COLLECTION(, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGLength)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGLength)
  tmp->CleanupWeakRefs();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGLength)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGLength)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGLength, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGLength, Release)

DOMSVGLength::DOMSVGLength(DOMSVGLengthList* aList, uint8_t aAttrEnum,
                           uint32_t aListIndex, bool aIsAnimValItem)
    : mOwner(aList),
      mListIndex(aListIndex),
      mAttrEnum(aAttrEnum),
      mIsAnimValItem(aIsAnimValItem),
      mUnit(SVGLength_Binding::SVG_LENGTHTYPE_NUMBER) {
  MOZ_ASSERT(aList, "bad arg");
  MOZ_ASSERT(mAttrEnum == aAttrEnum, "bitfield too small");
  MOZ_ASSERT(aListIndex <= MaxListIndex(), "list index too large");
  MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGLength::DOMSVGLength()
    : mOwner(nullptr),
      mListIndex(0),
      mAttrEnum(0),
      mIsAnimValItem(false),
      mUnit(SVGLength_Binding::SVG_LENGTHTYPE_NUMBER) {}

DOMSVGLength::DOMSVGLength(SVGAnimatedLength* aVal, SVGElement* aSVGElement,
                           bool aAnimVal)
    : mOwner(aSVGElement),
      mListIndex(0),
      mAttrEnum(aVal->mAttrEnum),
      mIsAnimValItem(aAnimVal),
      mUnit(SVGLength_Binding::SVG_LENGTHTYPE_NUMBER) {
  MOZ_ASSERT(aVal, "bad arg");
  MOZ_ASSERT(mAttrEnum == aVal->mAttrEnum, "bitfield too small");
}

void DOMSVGLength::CleanupWeakRefs() {
  // Our mList's weak ref to us must be nulled out when we die (or when we're
  // cycle collected), so we that don't leave behind a pointer to
  // free / soon-to-be-free memory.
  if (nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner)) {
    MOZ_ASSERT(lengthList->mItems[mListIndex] == this,
               "Clearing out the wrong list index...?");
    lengthList->mItems[mListIndex] = nullptr;
  }

  // Similarly, we must update the tearoff table to remove its (non-owning)
  // pointer to mVal.
  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    auto& table = mIsAnimValItem ? sAnimSVGLengthTearOffTable
                                 : sBaseSVGLengthTearOffTable;
    table.RemoveTearoff(svg->GetAnimatedLength(mAttrEnum));
  }
}

already_AddRefed<DOMSVGLength> DOMSVGLength::GetTearOff(SVGAnimatedLength* aVal,
                                                        SVGElement* aSVGElement,
                                                        bool aAnimVal) {
  auto& table =
      aAnimVal ? sAnimSVGLengthTearOffTable : sBaseSVGLengthTearOffTable;
  RefPtr<DOMSVGLength> domLength = table.GetTearoff(aVal);
  if (!domLength) {
    domLength = new DOMSVGLength(aVal, aSVGElement, aAnimVal);
    table.AddTearoff(aVal, domLength);
  }

  return domLength.forget();
}

DOMSVGLength* DOMSVGLength::Copy() {
  NS_ASSERTION(HasOwner(), "unexpected caller");
  DOMSVGLength* copy = new DOMSVGLength();
  uint16_t unit;
  float value;
  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    SVGAnimatedLength* length = svg->GetAnimatedLength(mAttrEnum);
    unit = length->GetSpecifiedUnitType();
    value = mIsAnimValItem ? length->GetAnimValInSpecifiedUnits()
                           : length->GetBaseValInSpecifiedUnits();
  } else {
    const SVGLength& length = InternalItem();
    unit = length.GetUnit();
    value = length.GetValueInCurrentUnits();
  }
  copy->NewValueSpecifiedUnits(unit, value, IgnoreErrors());
  return copy;
}

uint16_t DOMSVGLength::UnitType() {
  if (mIsAnimValItem) {
    Element()->FlushAnimations();
  }
  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    return svg->GetAnimatedLength(mAttrEnum)->GetSpecifiedUnitType();
  }
  return HasOwner() ? InternalItem().GetUnit() : mUnit;
}

float DOMSVGLength::GetValue(ErrorResult& aRv) {
  if (mIsAnimValItem) {
    Element()->FlushAnimations();  // May make HasOwner() == false
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    SVGAnimatedLength* length = svg->GetAnimatedLength(mAttrEnum);
    return mIsAnimValItem ? length->GetAnimValue(svg)
                          : length->GetBaseValue(svg);
  }

  if (nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner)) {
    float value = InternalItem().GetValueInUserUnits(lengthList->Element(),
                                                     lengthList->Axis());
    if (!IsFinite(value)) {
      aRv.Throw(NS_ERROR_FAILURE);
    }
    return value;
  }

  float unitToPx;
  if (UserSpaceMetrics::ResolveAbsoluteUnit(mUnit, unitToPx)) {
    return mValue * unitToPx;
  }

  // else [SVGWG issue] Can't convert this length's value to user units
  // ReportToConsole
  aRv.Throw(NS_ERROR_FAILURE);
  return 0.0f;
}

void DOMSVGLength::SetValue(float aUserUnitValue, ErrorResult& aRv) {
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    aRv = svg->GetAnimatedLength(mAttrEnum)->SetBaseValue(aUserUnitValue, svg,
                                                          true);
    return;
  }

  // Although the value passed in is in user units, this method does not turn
  // this length into a user unit length. Instead it converts the user unit
  // value to this length's current unit and sets that, leaving this length's
  // unit as it is.

  if (nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner)) {
    SVGLength& internalItem = InternalItem();
    if (internalItem.GetValueInUserUnits(
            lengthList->Element(), lengthList->Axis()) == aUserUnitValue) {
      return;
    }
    float uuPerUnit = internalItem.GetUserUnitsPerUnit(lengthList->Element(),
                                                       lengthList->Axis());
    if (uuPerUnit > 0) {
      float newValue = aUserUnitValue / uuPerUnit;
      if (IsFinite(newValue)) {
        AutoChangeLengthListNotifier notifier(this);
        internalItem.SetValueAndUnit(newValue, internalItem.GetUnit());
        return;
      }
    }
  } else if (mUnit == SVGLength_Binding::SVG_LENGTHTYPE_NUMBER ||
             mUnit == SVGLength_Binding::SVG_LENGTHTYPE_PX) {
    mValue = aUserUnitValue;
    return;
  }
  // else [SVGWG issue] Can't convert user unit value to this length's unit
  // ReportToConsole
  aRv.Throw(NS_ERROR_FAILURE);
}

float DOMSVGLength::ValueInSpecifiedUnits() {
  if (mIsAnimValItem) {
    Element()->FlushAnimations();  // May make HasOwner() == false
  }
  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    SVGAnimatedLength* length = svg->GetAnimatedLength(mAttrEnum);
    return mIsAnimValItem ? length->GetAnimValInSpecifiedUnits()
                          : length->GetBaseValInSpecifiedUnits();
  }

  return HasOwner() ? InternalItem().GetValueInCurrentUnits() : mValue;
}

void DOMSVGLength::SetValueInSpecifiedUnits(float aValue, ErrorResult& aRv) {
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    svg->GetAnimatedLength(mAttrEnum)->SetBaseValueInSpecifiedUnits(aValue, svg,
                                                                    true);
    return;
  }

  if (HasOwner()) {
    SVGLength& internalItem = InternalItem();
    if (internalItem.GetValueInCurrentUnits() == aValue) {
      return;
    }
    AutoChangeLengthListNotifier notifier(this);
    internalItem.SetValueInCurrentUnits(aValue);
    return;
  }
  mValue = aValue;
}

void DOMSVGLength::SetValueAsString(const nsAString& aValue, ErrorResult& aRv) {
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    aRv = svg->GetAnimatedLength(mAttrEnum)->SetBaseValueString(aValue, svg,
                                                                true);
    return;
  }

  SVGLength value;
  if (!value.SetValueFromString(aValue)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  if (HasOwner()) {
    SVGLength& internalItem = InternalItem();
    if (internalItem == value) {
      return;
    }
    AutoChangeLengthListNotifier notifier(this);
    internalItem = value;
    return;
  }
  mValue = value.GetValueInCurrentUnits();
  mUnit = value.GetUnit();
}

void DOMSVGLength::GetValueAsString(nsAString& aValue) {
  if (mIsAnimValItem) {
    Element()->FlushAnimations();  // May make HasOwner() == false
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    SVGAnimatedLength* length = svg->GetAnimatedLength(mAttrEnum);
    if (mIsAnimValItem) {
      length->GetAnimValueString(aValue);
    } else {
      length->GetBaseValueString(aValue);
    }
    return;
  }
  if (HasOwner()) {
    InternalItem().GetValueAsString(aValue);
    return;
  }
  SVGLength(mValue, mUnit).GetValueAsString(aValue);
}

void DOMSVGLength::NewValueSpecifiedUnits(uint16_t aUnit, float aValue,
                                          ErrorResult& aRv) {
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    svg->GetAnimatedLength(mAttrEnum)->NewValueSpecifiedUnits(aUnit, aValue,
                                                              svg);
    return;
  }

  if (!SVGLength::IsValidUnitType(aUnit)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }
  if (HasOwner()) {
    SVGLength& internalItem = InternalItem();
    if (internalItem == SVGLength(aValue, aUnit)) {
      return;
    }
    AutoChangeLengthListNotifier notifier(this);
    internalItem.SetValueAndUnit(aValue, uint8_t(aUnit));
    return;
  }
  mUnit = uint8_t(aUnit);
  mValue = aValue;
}

void DOMSVGLength::ConvertToSpecifiedUnits(uint16_t aUnit, ErrorResult& aRv) {
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    svg->GetAnimatedLength(mAttrEnum)->ConvertToSpecifiedUnits(aUnit, svg);
    return;
  }

  if (!SVGLength::IsValidUnitType(aUnit)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  float val;
  if (nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner)) {
    SVGLength& length = InternalItem();
    if (length.GetUnit() == aUnit) {
      return;
    }
    val = length.GetValueInSpecifiedUnit(aUnit, lengthList->Element(),
                                         lengthList->Axis());
  } else {
    val = SVGLength(mValue, mUnit).GetValueInSpecifiedUnit(aUnit, nullptr, 0);
  }
  if (IsFinite(val)) {
    if (HasOwner()) {
      AutoChangeLengthListNotifier notifier(this);
      InternalItem().SetValueAndUnit(val, aUnit);
    } else {
      mValue = val;
      mUnit = aUnit;
    }
    return;
  }
  // else [SVGWG issue] Can't convert unit
  // ReportToConsole
  aRv.Throw(NS_ERROR_FAILURE);
}

JSObject* DOMSVGLength::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGLength_Binding::Wrap(aCx, this, aGivenProto);
}

void DOMSVGLength::InsertingIntoList(DOMSVGLengthList* aList, uint8_t aAttrEnum,
                                     uint32_t aListIndex, bool aIsAnimValItem) {
  NS_ASSERTION(!HasOwner(), "Inserting item that is already in a list");

  mOwner = aList;
  mAttrEnum = aAttrEnum;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;

  MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGLength!");
}

void DOMSVGLength::RemovingFromList() {
  mValue = InternalItem().GetValueInCurrentUnits();
  mUnit = InternalItem().GetUnit();
  mOwner = nullptr;
  mIsAnimValItem = false;
}

SVGLength DOMSVGLength::ToSVGLength() {
  if (nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner)) {
    SVGAnimatedLength* length = svg->GetAnimatedLength(mAttrEnum);
    return SVGLength(mIsAnimValItem ? length->GetAnimValInSpecifiedUnits()
                                    : length->GetBaseValInSpecifiedUnits(),
                     length->GetSpecifiedUnitType());
  }
  return HasOwner() ? InternalItem() : SVGLength(mValue, mUnit);
}

bool DOMSVGLength::IsAnimating() const {
  if (nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner)) {
    return lengthList->IsAnimating();
  }
  nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner);
  return svg && svg->GetAnimatedLength(mAttrEnum)->IsAnimated();
}

SVGElement* DOMSVGLength::Element() {
  if (nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner)) {
    return lengthList->Element();
  }
  nsCOMPtr<SVGElement> svg = do_QueryInterface(mOwner);
  return svg;
}

SVGLength& DOMSVGLength::InternalItem() {
  nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner);
  SVGAnimatedLengthList* alist =
      lengthList->Element()->GetAnimatedLengthList(mAttrEnum);
  return mIsAnimValItem && alist->mAnimVal ? (*alist->mAnimVal)[mListIndex]
                                           : alist->mBaseVal[mListIndex];
}

#ifdef DEBUG
bool DOMSVGLength::IndexIsValid() {
  nsCOMPtr<DOMSVGLengthList> lengthList = do_QueryInterface(mOwner);
  SVGAnimatedLengthList* alist =
      lengthList->Element()->GetAnimatedLengthList(mAttrEnum);
  return (mIsAnimValItem && mListIndex < alist->GetAnimValue().Length()) ||
         (!mIsAnimValItem && mListIndex < alist->GetBaseValue().Length());
}
#endif

}  // namespace dom
}  // namespace mozilla
