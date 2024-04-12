/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGElement.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/dom/CSSRuleBinding.h"
#include "mozilla/dom/SVGElementBinding.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGTests.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "mozilla/dom/Element.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/PresShell.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/SMILAnimationController.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/Unused.h"

#include "mozAutoDocUpdate.h"
#include "nsAttrValueOrString.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsContentUtils.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsICSSDeclaration.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsQueryObject.h"
#include "nsLayoutUtils.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedPointList.h"
#include "SVGAnimatedPathSegList.h"
#include "SVGAnimatedTransformList.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedIntegerPair.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGAnimatedOrient.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedViewBox.h"
#include "SVGGeometryProperty.h"
#include "SVGMotionSMILAttr.h"
#include <stdarg.h>

// This is needed to ensure correct handling of calls to the
// vararg-list methods in this file:
//   SVGElement::GetAnimated{Length,Number,Integer}Values
// See bug 547964 for details:
static_assert(sizeof(void*) == sizeof(nullptr),
              "nullptr should be the correct size");

nsresult NS_NewSVGElement(
    mozilla::dom::Element** aResult,
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  auto* nim = nodeInfo->NodeInfoManager();
  RefPtr<mozilla::dom::SVGElement> it =
      new (nim) mozilla::dom::SVGElement(nodeInfo.forget());
  nsresult rv = it->Init();

  if (NS_FAILED(rv)) {
    return rv;
  }

  it.forget(aResult);
  return rv;
}

namespace mozilla::dom {
using namespace SVGUnitTypes_Binding;

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGElement)

// Use the CC variant of this, even though this class does not define
// a new CC participant, to make QIing to the CC interfaces faster.
NS_IMPL_QUERY_INTERFACE_CYCLE_COLLECTION_INHERITED(SVGElement, SVGElementBase,
                                                   SVGElement)

SVGEnumMapping SVGElement::sSVGUnitTypesMap[] = {
    {nsGkAtoms::userSpaceOnUse, SVG_UNIT_TYPE_USERSPACEONUSE},
    {nsGkAtoms::objectBoundingBox, SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
    {nullptr, 0}};

SVGElement::SVGElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGElementBase(std::move(aNodeInfo)) {}

SVGElement::~SVGElement() = default;

JSObject* SVGElement::WrapNode(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return SVGElement_Binding::Wrap(aCx, this, aGivenProto);
}

template <typename Value, typename Info>
void SVGElement::AttributesInfo<Value, Info>::ResetAll() {
  for (uint32_t i = 0; i < mCount; ++i) {
    Reset(i);
  }
}

template <typename Value, typename Info>
void SVGElement::AttributesInfo<Value, Info>::CopyAllFrom(
    const AttributesInfo& aOther) {
  MOZ_DIAGNOSTIC_ASSERT(mCount == aOther.mCount,
                        "Should only be called on clones");
  for (uint32_t i = 0; i < mCount; ++i) {
    mValues[i] = aOther.mValues[i];
  }
}

template <>
void SVGElement::LengthAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(mInfos[aAttrEnum].mCtxType, aAttrEnum,
                          mInfos[aAttrEnum].mDefaultValue,
                          mInfos[aAttrEnum].mDefaultUnitType);
}

template <>
void SVGElement::LengthListAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].ClearBaseValue(aAttrEnum);
  // caller notifies
}

template <>
void SVGElement::NumberListAttributesInfo::Reset(uint8_t aAttrEnum) {
  MOZ_ASSERT(aAttrEnum < mCount, "Bad attr enum");
  mValues[aAttrEnum].ClearBaseValue(aAttrEnum);
  // caller notifies
}

template <>
void SVGElement::NumberAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum, mInfos[aAttrEnum].mDefaultValue);
}

template <>
void SVGElement::NumberPairAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum, mInfos[aAttrEnum].mDefaultValue1,
                          mInfos[aAttrEnum].mDefaultValue2);
}

template <>
void SVGElement::IntegerAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum, mInfos[aAttrEnum].mDefaultValue);
}

template <>
void SVGElement::IntegerPairAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum, mInfos[aAttrEnum].mDefaultValue1,
                          mInfos[aAttrEnum].mDefaultValue2);
}

template <>
void SVGElement::BooleanAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum, mInfos[aAttrEnum].mDefaultValue);
}

template <>
void SVGElement::StringListAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Clear();
  // caller notifies
}

template <>
void SVGElement::EnumAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum, mInfos[aAttrEnum].mDefaultValue);
}

template <>
void SVGElement::StringAttributesInfo::Reset(uint8_t aAttrEnum) {
  mValues[aAttrEnum].Init(aAttrEnum);
}

nsresult SVGElement::CopyInnerTo(mozilla::dom::Element* aDest) {
  nsresult rv = Element::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  auto* dest = static_cast<SVGElement*>(aDest);

  // cloning a node must retain its internal nonce slot
  if (auto* nonce = static_cast<nsString*>(GetProperty(nsGkAtoms::nonce))) {
    dest->SetNonce(*nonce);
  }

  // If our destination is a print document, copy all the relevant length values
  // etc so that they match the state of the original node.
  if (aDest->OwnerDoc()->IsStaticDocument() ||
      aDest->OwnerDoc()->CloningForSVGUse()) {
    LengthAttributesInfo lengthInfo = GetLengthInfo();
    dest->GetLengthInfo().CopyAllFrom(lengthInfo);
    if (SVGGeometryProperty::ElementMapsLengthsToStyle(this)) {
      for (uint32_t i = 0; i < lengthInfo.mCount; i++) {
        nsCSSPropertyID propId =
            SVGGeometryProperty::AttrEnumToCSSPropId(this, i);

        // We don't map use element width/height currently. We can remove this
        // test when we do.
        if (propId != eCSSProperty_UNKNOWN &&
            lengthInfo.mValues[i].IsAnimated()) {
          dest->SMILOverrideStyle()->SetSMILValue(propId,
                                                  lengthInfo.mValues[i]);
        }
      }
    }
    dest->GetNumberInfo().CopyAllFrom(GetNumberInfo());
    dest->GetNumberPairInfo().CopyAllFrom(GetNumberPairInfo());
    dest->GetIntegerInfo().CopyAllFrom(GetIntegerInfo());
    dest->GetIntegerPairInfo().CopyAllFrom(GetIntegerPairInfo());
    dest->GetBooleanInfo().CopyAllFrom(GetBooleanInfo());
    if (const auto* orient = GetAnimatedOrient()) {
      *dest->GetAnimatedOrient() = *orient;
    }
    if (const auto* viewBox = GetAnimatedViewBox()) {
      *dest->GetAnimatedViewBox() = *viewBox;
    }
    if (const auto* preserveAspectRatio = GetAnimatedPreserveAspectRatio()) {
      *dest->GetAnimatedPreserveAspectRatio() = *preserveAspectRatio;
    }
    dest->GetEnumInfo().CopyAllFrom(GetEnumInfo());
    dest->GetStringInfo().CopyAllFrom(GetStringInfo());
    dest->GetLengthListInfo().CopyAllFrom(GetLengthListInfo());
    dest->GetNumberListInfo().CopyAllFrom(GetNumberListInfo());
    if (const auto* pointList = GetAnimatedPointList()) {
      *dest->GetAnimatedPointList() = *pointList;
    }
    if (const auto* pathSegList = GetAnimPathSegList()) {
      *dest->GetAnimPathSegList() = *pathSegList;
      if (pathSegList->IsAnimating()) {
        dest->SMILOverrideStyle()->SetSMILValue(nsCSSPropertyID::eCSSProperty_d,
                                                *pathSegList);
      }
    }
    if (const auto* transformList = GetAnimatedTransformList()) {
      *dest->GetAnimatedTransformList(DO_ALLOCATE) = *transformList;
    }
    if (const auto* animateMotionTransform = GetAnimateMotionTransform()) {
      dest->SetAnimateMotionTransform(animateMotionTransform);
    }
    if (const auto* smilOverrideStyleDecoration =
            GetSMILOverrideStyleDeclaration()) {
      RefPtr<DeclarationBlock> declClone = smilOverrideStyleDecoration->Clone();
      declClone->SetDirty();
      dest->SetSMILOverrideStyleDeclaration(*declClone);
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// SVGElement methods

void SVGElement::DidAnimateClass() {
  // For Servo, snapshot the element before we change it.
  PresShell* presShell = OwnerDoc()->GetPresShell();
  if (presShell) {
    if (nsPresContext* presContext = presShell->GetPresContext()) {
      presContext->RestyleManager()->ClassAttributeWillBeChangedBySMIL(this);
    }
  }

  nsAutoString src;
  mClassAttribute.GetAnimValue(src, this);
  if (!mClassAnimAttr) {
    mClassAnimAttr = MakeUnique<nsAttrValue>();
  }
  mClassAnimAttr->ParseAtomArray(src);

  // FIXME(emilio): This re-selector-matches, but we do the snapshot stuff right
  // above... Is this needed anymore?
  if (presShell) {
    presShell->RestyleForAnimation(this, RestyleHint::RESTYLE_SELF);
  }
  DidAnimateAttribute(kNameSpaceID_None, nsGkAtoms::_class);
}

nsresult SVGElement::Init() {
  // Set up length attributes - can't do this in the constructor
  // because we can't do a virtual call at that point

  GetLengthInfo().ResetAll();
  GetNumberInfo().ResetAll();
  GetNumberPairInfo().ResetAll();
  GetIntegerInfo().ResetAll();
  GetIntegerPairInfo().ResetAll();
  GetBooleanInfo().ResetAll();
  GetEnumInfo().ResetAll();

  if (SVGAnimatedOrient* orient = GetAnimatedOrient()) {
    orient->Init();
  }

  if (SVGAnimatedViewBox* viewBox = GetAnimatedViewBox()) {
    viewBox->Init();
  }

  if (SVGAnimatedPreserveAspectRatio* preserveAspectRatio =
          GetAnimatedPreserveAspectRatio()) {
    preserveAspectRatio->Init();
  }

  GetLengthListInfo().ResetAll();
  GetNumberListInfo().ResetAll();

  // No need to reset SVGPointList since the default value is always the same
  // (an empty list).

  // No need to reset SVGPathData since the default value is always the same
  // (an empty list).

  GetStringInfo().ResetAll();
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation

//----------------------------------------------------------------------
// nsIContent methods

nsresult SVGElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = SVGElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Hide any nonce from the DOM, but keep the internal value of the
  // nonce by copying and resetting the internal nonce value.
  if (HasFlag(NODE_HAS_NONCE_AND_HEADER_CSP) && IsInComposedDoc() &&
      OwnerDoc()->GetBrowsingContext()) {
    nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
        "SVGElement::ResetNonce::Runnable",
        [self = RefPtr<SVGElement>(this)]() {
          nsAutoString nonce;
          self->GetNonce(nonce);
          self->SetAttr(kNameSpaceID_None, nsGkAtoms::nonce, u""_ns, true);
          self->SetNonce(nonce);
        }));
  }

  return NS_OK;
}

void SVGElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal, bool aNotify) {
  if (IsEventAttributeName(aName) && aValue) {
    MOZ_ASSERT(aValue->Type() == nsAttrValue::eString,
               "Expected string value for script body");
    SetEventHandler(GetEventNameForAttr(aName), aValue->GetStringValue());
  }

  // The nonce will be copied over to an internal slot and cleared from the
  // Element within BindToTree to avoid CSS Selector nonce exfiltration if
  // the CSP list contains a header-delivered CSP.
  if (nsGkAtoms::nonce == aName && kNameSpaceID_None == aNamespaceID) {
    if (aValue) {
      SetNonce(aValue->GetStringValue());
      if (OwnerDoc()->GetHasCSPDeliveredThroughHeader()) {
        SetFlags(NODE_HAS_NONCE_AND_HEADER_CSP);
      }
    } else {
      RemoveNonce();
    }
  }

  return SVGElementBase::AfterSetAttr(aNamespaceID, aName, aValue, aOldValue,
                                      aSubjectPrincipal, aNotify);
}

bool SVGElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult) {
  nsresult rv = NS_OK;
  bool foundMatch = false;
  bool didSetResult = false;

  if (aNamespaceID == kNameSpaceID_None) {
    // Check for SVGAnimatedLength attribute
    LengthAttributesInfo lengthInfo = GetLengthInfo();

    uint32_t i;
    for (i = 0; i < lengthInfo.mCount; i++) {
      if (aAttribute == lengthInfo.mInfos[i].mName) {
        rv = lengthInfo.mValues[i].SetBaseValueString(aValue, this, false);
        if (NS_FAILED(rv)) {
          lengthInfo.Reset(i);
        } else {
          aResult.SetTo(lengthInfo.mValues[i], &aValue);
          didSetResult = true;
        }
        foundMatch = true;
        break;
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedLengthList attribute
      LengthListAttributesInfo lengthListInfo = GetLengthListInfo();
      for (i = 0; i < lengthListInfo.mCount; i++) {
        if (aAttribute == lengthListInfo.mInfos[i].mName) {
          rv = lengthListInfo.mValues[i].SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            lengthListInfo.Reset(i);
          } else {
            aResult.SetTo(lengthListInfo.mValues[i].GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedNumberList attribute
      NumberListAttributesInfo numberListInfo = GetNumberListInfo();
      for (i = 0; i < numberListInfo.mCount; i++) {
        if (aAttribute == numberListInfo.mInfos[i].mName) {
          rv = numberListInfo.mValues[i].SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            numberListInfo.Reset(i);
          } else {
            aResult.SetTo(numberListInfo.mValues[i].GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedPointList attribute
      if (GetPointListAttrName() == aAttribute) {
        if (SVGAnimatedPointList* pointList = GetAnimatedPointList()) {
          pointList->SetBaseValueString(aValue);
          // The spec says we parse everything up to the failure, so we DON'T
          // need to check the result of SetBaseValueString or call
          // pointList->ClearBaseValue() if it fails
          aResult.SetTo(pointList->GetBaseValue(), &aValue);
          didSetResult = true;
          foundMatch = true;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedPathSegList attribute
      if (GetPathDataAttrName() == aAttribute) {
        if (SVGAnimatedPathSegList* segList = GetAnimPathSegList()) {
          segList->SetBaseValueString(aValue);
          // The spec says we parse everything up to the failure, so we DON'T
          // need to check the result of SetBaseValueString or call
          // segList->ClearBaseValue() if it fails
          aResult.SetTo(segList->GetBaseValue(), &aValue);
          didSetResult = true;
          foundMatch = true;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedNumber attribute
      NumberAttributesInfo numberInfo = GetNumberInfo();
      for (i = 0; i < numberInfo.mCount; i++) {
        if (aAttribute == numberInfo.mInfos[i].mName) {
          rv = numberInfo.mValues[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            numberInfo.Reset(i);
          } else {
            aResult.SetTo(numberInfo.mValues[i].GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedNumberPair attribute
      NumberPairAttributesInfo numberPairInfo = GetNumberPairInfo();
      for (i = 0; i < numberPairInfo.mCount; i++) {
        if (aAttribute == numberPairInfo.mInfos[i].mName) {
          rv = numberPairInfo.mValues[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            numberPairInfo.Reset(i);
          } else {
            aResult.SetTo(numberPairInfo.mValues[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedInteger attribute
      IntegerAttributesInfo integerInfo = GetIntegerInfo();
      for (i = 0; i < integerInfo.mCount; i++) {
        if (aAttribute == integerInfo.mInfos[i].mName) {
          rv = integerInfo.mValues[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            integerInfo.Reset(i);
          } else {
            aResult.SetTo(integerInfo.mValues[i].GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedIntegerPair attribute
      IntegerPairAttributesInfo integerPairInfo = GetIntegerPairInfo();
      for (i = 0; i < integerPairInfo.mCount; i++) {
        if (aAttribute == integerPairInfo.mInfos[i].mName) {
          rv = integerPairInfo.mValues[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            integerPairInfo.Reset(i);
          } else {
            aResult.SetTo(integerPairInfo.mValues[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedBoolean attribute
      BooleanAttributesInfo booleanInfo = GetBooleanInfo();
      for (i = 0; i < booleanInfo.mCount; i++) {
        if (aAttribute == booleanInfo.mInfos[i].mName) {
          nsAtom* valAtom = NS_GetStaticAtom(aValue);
          rv = valAtom ? booleanInfo.mValues[i].SetBaseValueAtom(valAtom, this)
                       : NS_ERROR_DOM_SYNTAX_ERR;
          if (NS_FAILED(rv)) {
            booleanInfo.Reset(i);
          } else {
            aResult.SetTo(valAtom);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedEnumeration attribute
      EnumAttributesInfo enumInfo = GetEnumInfo();
      for (i = 0; i < enumInfo.mCount; i++) {
        if (aAttribute == enumInfo.mInfos[i].mName) {
          RefPtr<nsAtom> valAtom = NS_Atomize(aValue);
          if (!enumInfo.mValues[i].SetBaseValueAtom(valAtom, this)) {
            // Exact error value does not matter; we just need to mark the
            // parse as failed.
            rv = NS_ERROR_FAILURE;
            enumInfo.Reset(i);
          } else {
            aResult.SetTo(valAtom);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for conditional processing attributes
      nsCOMPtr<SVGTests> tests = do_QueryObject(this);
      if (tests && tests->ParseConditionalProcessingAttribute(
                       aAttribute, aValue, aResult)) {
        foundMatch = true;
      }
    }

    if (!foundMatch) {
      // Check for StringList attribute
      StringListAttributesInfo stringListInfo = GetStringListInfo();
      for (i = 0; i < stringListInfo.mCount; i++) {
        if (aAttribute == stringListInfo.mInfos[i].mName) {
          rv = stringListInfo.mValues[i].SetValue(aValue);
          if (NS_FAILED(rv)) {
            stringListInfo.Reset(i);
          } else {
            aResult.SetTo(stringListInfo.mValues[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for orient attribute
      if (aAttribute == nsGkAtoms::orient) {
        SVGAnimatedOrient* orient = GetAnimatedOrient();
        if (orient) {
          rv = orient->SetBaseValueString(aValue, this, false);
          if (NS_FAILED(rv)) {
            orient->Init();
          } else {
            aResult.SetTo(*orient, &aValue);
            didSetResult = true;
          }
          foundMatch = true;
        }
        // Check for viewBox attribute
      } else if (aAttribute == nsGkAtoms::viewBox) {
        SVGAnimatedViewBox* viewBox = GetAnimatedViewBox();
        if (viewBox) {
          rv = viewBox->SetBaseValueString(aValue, this, false);
          if (NS_FAILED(rv)) {
            viewBox->Init();
          } else {
            aResult.SetTo(*viewBox, &aValue);
            didSetResult = true;
          }
          foundMatch = true;
        }
        // Check for preserveAspectRatio attribute
      } else if (aAttribute == nsGkAtoms::preserveAspectRatio) {
        SVGAnimatedPreserveAspectRatio* preserveAspectRatio =
            GetAnimatedPreserveAspectRatio();
        if (preserveAspectRatio) {
          rv = preserveAspectRatio->SetBaseValueString(aValue, this, false);
          if (NS_FAILED(rv)) {
            preserveAspectRatio->Init();
          } else {
            aResult.SetTo(*preserveAspectRatio, &aValue);
            didSetResult = true;
          }
          foundMatch = true;
        }
        // Check for SVGAnimatedTransformList attribute
      } else if (GetTransformListAttrName() == aAttribute) {
        // The transform attribute is being set, so we must ensure that the
        // SVGAnimatedTransformList is/has been allocated:
        SVGAnimatedTransformList* transformList =
            GetAnimatedTransformList(DO_ALLOCATE);
        rv = transformList->SetBaseValueString(aValue, this);
        if (NS_FAILED(rv)) {
          transformList->ClearBaseValue();
        } else {
          aResult.SetTo(transformList->GetBaseValue(), &aValue);
          didSetResult = true;
        }
        foundMatch = true;
      } else if (aAttribute == nsGkAtoms::tabindex) {
        didSetResult = aResult.ParseIntValue(aValue);
        foundMatch = true;
      }
    }

    if (aAttribute == nsGkAtoms::_class) {
      mClassAttribute.SetBaseValue(aValue, this, false);
      aResult.ParseAtomArray(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::rel) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }

  if (!foundMatch) {
    // Check for SVGAnimatedString attribute
    StringAttributesInfo stringInfo = GetStringInfo();
    for (uint32_t i = 0; i < stringInfo.mCount; i++) {
      if (aNamespaceID == stringInfo.mInfos[i].mNamespaceID &&
          aAttribute == stringInfo.mInfos[i].mName) {
        stringInfo.mValues[i].SetBaseValue(aValue, this, false);
        foundMatch = true;
        break;
      }
    }
  }

  if (foundMatch) {
    if (NS_FAILED(rv)) {
      ReportAttributeParseFailure(OwnerDoc(), aAttribute, aValue);
      return false;
    }
    if (!didSetResult) {
      aResult.SetTo(aValue);
    }
    return true;
  }

  return SVGElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                        aMaybeScriptedPrincipal, aResult);
}

void SVGElement::UnsetAttrInternal(int32_t aNamespaceID, nsAtom* aName,
                                   bool aNotify) {
  // XXXbz there's a bunch of redundancy here with AfterSetAttr.
  // Maybe consolidate?

  if (aNamespaceID == kNameSpaceID_None) {
    if (IsEventAttributeName(aName)) {
      EventListenerManager* manager = GetExistingListenerManager();
      if (manager) {
        nsAtom* eventName = GetEventNameForAttr(aName);
        manager->RemoveEventHandler(eventName);
      }
      return;
    }

    // Check if this is a length attribute going away
    LengthAttributesInfo lenInfo = GetLengthInfo();

    for (uint32_t i = 0; i < lenInfo.mCount; i++) {
      if (aName == lenInfo.mInfos[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        lenInfo.Reset(i);
        return;
      }
    }

    // Check if this is a length list attribute going away
    LengthListAttributesInfo lengthListInfo = GetLengthListInfo();

    for (uint32_t i = 0; i < lengthListInfo.mCount; i++) {
      if (aName == lengthListInfo.mInfos[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        lengthListInfo.Reset(i);
        return;
      }
    }

    // Check if this is a number list attribute going away
    NumberListAttributesInfo numberListInfo = GetNumberListInfo();

    for (uint32_t i = 0; i < numberListInfo.mCount; i++) {
      if (aName == numberListInfo.mInfos[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        numberListInfo.Reset(i);
        return;
      }
    }

    // Check if this is a point list attribute going away
    if (GetPointListAttrName() == aName) {
      SVGAnimatedPointList* pointList = GetAnimatedPointList();
      if (pointList) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        pointList->ClearBaseValue();
        return;
      }
    }

    // Check if this is a path segment list attribute going away
    if (GetPathDataAttrName() == aName) {
      SVGAnimatedPathSegList* segList = GetAnimPathSegList();
      if (segList) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        segList->ClearBaseValue();
        return;
      }
    }

    // Check if this is a number attribute going away
    NumberAttributesInfo numInfo = GetNumberInfo();

    for (uint32_t i = 0; i < numInfo.mCount; i++) {
      if (aName == numInfo.mInfos[i].mName) {
        numInfo.Reset(i);
        return;
      }
    }

    // Check if this is a number pair attribute going away
    NumberPairAttributesInfo numPairInfo = GetNumberPairInfo();

    for (uint32_t i = 0; i < numPairInfo.mCount; i++) {
      if (aName == numPairInfo.mInfos[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        numPairInfo.Reset(i);
        return;
      }
    }

    // Check if this is an integer attribute going away
    IntegerAttributesInfo intInfo = GetIntegerInfo();

    for (uint32_t i = 0; i < intInfo.mCount; i++) {
      if (aName == intInfo.mInfos[i].mName) {
        intInfo.Reset(i);
        return;
      }
    }

    // Check if this is an integer pair attribute going away
    IntegerPairAttributesInfo intPairInfo = GetIntegerPairInfo();

    for (uint32_t i = 0; i < intPairInfo.mCount; i++) {
      if (aName == intPairInfo.mInfos[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        intPairInfo.Reset(i);
        return;
      }
    }

    // Check if this is a boolean attribute going away
    BooleanAttributesInfo boolInfo = GetBooleanInfo();

    for (uint32_t i = 0; i < boolInfo.mCount; i++) {
      if (aName == boolInfo.mInfos[i].mName) {
        boolInfo.Reset(i);
        return;
      }
    }

    // Check if this is an enum attribute going away
    EnumAttributesInfo enumInfo = GetEnumInfo();

    for (uint32_t i = 0; i < enumInfo.mCount; i++) {
      if (aName == enumInfo.mInfos[i].mName) {
        enumInfo.Reset(i);
        return;
      }
    }

    // Check if this is an orient attribute going away
    if (aName == nsGkAtoms::orient) {
      SVGAnimatedOrient* orient = GetAnimatedOrient();
      if (orient) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        orient->Init();
        return;
      }
    }

    // Check if this is a viewBox attribute going away
    if (aName == nsGkAtoms::viewBox) {
      SVGAnimatedViewBox* viewBox = GetAnimatedViewBox();
      if (viewBox) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        viewBox->Init();
        return;
      }
    }

    // Check if this is a preserveAspectRatio attribute going away
    if (aName == nsGkAtoms::preserveAspectRatio) {
      SVGAnimatedPreserveAspectRatio* preserveAspectRatio =
          GetAnimatedPreserveAspectRatio();
      if (preserveAspectRatio) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        preserveAspectRatio->Init();
        return;
      }
    }

    // Check if this is a transform list attribute going away
    if (GetTransformListAttrName() == aName) {
      SVGAnimatedTransformList* transformList = GetAnimatedTransformList();
      if (transformList) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        transformList->ClearBaseValue();
        return;
      }
    }

    // Check for conditional processing attributes
    nsCOMPtr<SVGTests> tests = do_QueryObject(this);
    if (tests && tests->IsConditionalProcessingAttribute(aName)) {
      MaybeSerializeAttrBeforeRemoval(aName, aNotify);
      tests->UnsetAttr(aName);
      return;
    }

    // Check if this is a string list attribute going away
    StringListAttributesInfo stringListInfo = GetStringListInfo();

    for (uint32_t i = 0; i < stringListInfo.mCount; i++) {
      if (aName == stringListInfo.mInfos[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        stringListInfo.Reset(i);
        return;
      }
    }

    if (aName == nsGkAtoms::_class) {
      mClassAttribute.Init();
      return;
    }
  }

  // Check if this is a string attribute going away
  StringAttributesInfo stringInfo = GetStringInfo();

  for (uint32_t i = 0; i < stringInfo.mCount; i++) {
    if (aNamespaceID == stringInfo.mInfos[i].mNamespaceID &&
        aName == stringInfo.mInfos[i].mName) {
      stringInfo.Reset(i);
      return;
    }
  }
}

void SVGElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                               const nsAttrValue* aValue, bool aNotify) {
  if (!aValue) {
    UnsetAttrInternal(aNamespaceID, aName, aNotify);
  }
  return SVGElementBase::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
}

nsChangeHint SVGElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                int32_t aModType) const {
  nsChangeHint retval =
      SVGElementBase::GetAttributeChangeHint(aAttribute, aModType);

  nsCOMPtr<SVGTests> tests = do_QueryObject(const_cast<SVGElement*>(this));
  if (tests && tests->IsConditionalProcessingAttribute(aAttribute)) {
    // It would be nice to only reconstruct the frame if the value returned by
    // SVGTests::PassesConditionalProcessingTests has changed, but we don't
    // know that
    retval |= nsChangeHint_ReconstructFrame;
  }
  return retval;
}

void SVGElement::NodeInfoChanged(Document* aOldDoc) {
  SVGElementBase::NodeInfoChanged(aOldDoc);
}

NS_IMETHODIMP_(bool)
SVGElement::IsAttributeMapped(const nsAtom* name) const {
  if (name == nsGkAtoms::lang) {
    return true;
  }

  if (IsSVGAnimationElement()) {
    return SVGElementBase::IsAttributeMapped(name);
  }

  static const MappedAttributeEntry attributes[] = {
      // Properties that we don't support are commented out.
      // { nsGkAtoms::alignment_baseline },
      // { nsGkAtoms::baseline_shift },
      {nsGkAtoms::clip},
      {nsGkAtoms::clip_path},
      {nsGkAtoms::clip_rule},
      {nsGkAtoms::color},
      {nsGkAtoms::colorInterpolation},
      {nsGkAtoms::colorInterpolationFilters},
      {nsGkAtoms::cursor},
      {nsGkAtoms::direction},
      {nsGkAtoms::display},
      {nsGkAtoms::dominant_baseline},
      {nsGkAtoms::fill},
      {nsGkAtoms::fill_opacity},
      {nsGkAtoms::fill_rule},
      {nsGkAtoms::filter},
      {nsGkAtoms::flood_color},
      {nsGkAtoms::flood_opacity},
      {nsGkAtoms::font_family},
      {nsGkAtoms::font_size},
      {nsGkAtoms::font_size_adjust},
      {nsGkAtoms::font_stretch},
      {nsGkAtoms::font_style},
      {nsGkAtoms::font_variant},
      {nsGkAtoms::fontWeight},
      {nsGkAtoms::image_rendering},
      {nsGkAtoms::letter_spacing},
      {nsGkAtoms::lighting_color},
      {nsGkAtoms::marker_end},
      {nsGkAtoms::marker_mid},
      {nsGkAtoms::marker_start},
      {nsGkAtoms::mask},
      {nsGkAtoms::mask_type},
      {nsGkAtoms::opacity},
      {nsGkAtoms::overflow},
      {nsGkAtoms::paint_order},
      {nsGkAtoms::pointer_events},
      {nsGkAtoms::shape_rendering},
      {nsGkAtoms::stop_color},
      {nsGkAtoms::stop_opacity},
      {nsGkAtoms::stroke},
      {nsGkAtoms::stroke_dasharray},
      {nsGkAtoms::stroke_dashoffset},
      {nsGkAtoms::stroke_linecap},
      {nsGkAtoms::stroke_linejoin},
      {nsGkAtoms::stroke_miterlimit},
      {nsGkAtoms::stroke_opacity},
      {nsGkAtoms::stroke_width},
      {nsGkAtoms::text_anchor},
      {nsGkAtoms::text_decoration},
      {nsGkAtoms::text_rendering},
      {nsGkAtoms::transform_origin},
      {nsGkAtoms::unicode_bidi},
      {nsGkAtoms::vector_effect},
      {nsGkAtoms::visibility},
      {nsGkAtoms::white_space},
      {nsGkAtoms::word_spacing},
      {nsGkAtoms::writing_mode},
      {nullptr}};

  static const MappedAttributeEntry* const map[] = {attributes};

  return FindAttributeDependence(name, map) ||
         SVGElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// Element methods

// forwarded to Element implementations

//----------------------------------------------------------------------

SVGSVGElement* SVGElement::GetOwnerSVGElement() {
  nsIContent* ancestor = GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVGElement()) {
    if (ancestor->IsSVGElement(nsGkAtoms::foreignObject)) {
      return nullptr;
    }
    if (auto* svg = SVGSVGElement::FromNode(ancestor)) {
      return svg;
    }
    ancestor = ancestor->GetFlattenedTreeParent();
  }

  // we don't have an ancestor <svg> element...
  return nullptr;
}

SVGElement* SVGElement::GetViewportElement() {
  return SVGContentUtils::GetNearestViewportElement(this);
}

already_AddRefed<DOMSVGAnimatedString> SVGElement::ClassName() {
  return mClassAttribute.ToDOMAnimatedString(this);
}

/* static */
bool SVGElement::UpdateDeclarationBlockFromLength(
    StyleLockedDeclarationBlock& aBlock, nsCSSPropertyID aPropId,
    const SVGAnimatedLength& aLength, ValToUse aValToUse) {
  float value;
  uint8_t units;
  if (aValToUse == ValToUse::Anim) {
    value = aLength.GetAnimValInSpecifiedUnits();
    units = aLength.GetAnimUnitType();
  } else {
    MOZ_ASSERT(aValToUse == ValToUse::Base);
    value = aLength.GetBaseValInSpecifiedUnits();
    units = aLength.GetBaseUnitType();
  }

  // SVG parser doesn't check non-negativity of some parsed value, we should not
  // pass those to CSS side.
  if (value < 0 &&
      SVGGeometryProperty::IsNonNegativeGeometryProperty(aPropId)) {
    return false;
  }

  nsCSSUnit cssUnit = SVGLength::SpecifiedUnitTypeToCSSUnit(units);

  if (cssUnit == eCSSUnit_Percent) {
    Servo_DeclarationBlock_SetPercentValue(&aBlock, aPropId, value / 100.f);
  } else {
    Servo_DeclarationBlock_SetLengthValue(&aBlock, aPropId, value, cssUnit);
  }

  return true;
}

/* static */
bool SVGElement::UpdateDeclarationBlockFromPath(
    StyleLockedDeclarationBlock& aBlock, const SVGAnimatedPathSegList& aPath,
    ValToUse aValToUse) {
  const SVGPathData& pathData =
      aValToUse == ValToUse::Anim ? aPath.GetAnimValue() : aPath.GetBaseValue();

  // SVGPathData::mData is fallible but rust binding accepts nsTArray only, so
  // we need to point to one or the other. Fortunately, fallible and infallible
  // array types can be implicitly converted provided they are const.
  //
  // FIXME: here we just convert the data structure from cpp verion into rust
  // version. We don't do any normalization for the path data from d attribute.
  // Based on the current discussion of https://github.com/w3c/svgwg/issues/321,
  // we may have to convert the relative commands into absolute commands.
  // The normalization should be fixed in Bug 1489392. Besides, Bug 1714238
  // will use the same data structure, so we may simplify this more.
  const nsTArray<float>& asInFallibleArray = pathData.RawData();
  Servo_DeclarationBlock_SetPathValue(&aBlock, eCSSProperty_d,
                                      &asInFallibleArray);
  return true;
}

//------------------------------------------------------------------------
// Helper class: MappedAttrParser, for parsing values of mapped attributes

namespace {

class MOZ_STACK_CLASS MappedAttrParser {
 public:
  explicit MappedAttrParser(SVGElement& aElement,
                            StyleLockedDeclarationBlock* aDecl)
      : mElement(aElement), mDecl(aDecl) {
    if (mDecl) {
      Servo_DeclarationBlock_Clear(mDecl);
    }
  }
  ~MappedAttrParser() {
    MOZ_ASSERT(!mDecl,
               "If mDecl was initialized, it should have been returned via "
               "TakeDeclarationBlock (and have its pointer cleared)");
  };

  // Parses a mapped attribute value.
  void ParseMappedAttrValue(nsAtom* aMappedAttrName,
                            const nsAString& aMappedAttrValue);

  void TellStyleAlreadyParsedResult(nsAtom const* aAtom,
                                    SVGAnimatedLength const& aLength);
  void TellStyleAlreadyParsedResult(const SVGAnimatedPathSegList& aPath);

  // If we've parsed any values for mapped attributes, this method returns the
  // already_AddRefed declaration block that incorporates the parsed values.
  // Otherwise, this method returns null.
  already_AddRefed<StyleLockedDeclarationBlock> TakeDeclarationBlock() {
    return mDecl.forget();
  }

  StyleLockedDeclarationBlock& EnsureDeclarationBlock() {
    if (!mDecl) {
      mDecl = Servo_DeclarationBlock_CreateEmpty().Consume();
    }
    return *mDecl;
  }

  URLExtraData& EnsureExtraData() {
    if (!mExtraData) {
      mExtraData = mElement.GetURLDataForStyleAttr();
    }
    return *mExtraData;
  }

 private:
  // For reporting use counters
  SVGElement& mElement;

  // Declaration for storing parsed values (lazily initialized).
  RefPtr<StyleLockedDeclarationBlock> mDecl;

  // URL data for parsing stuff. Also lazy.
  RefPtr<URLExtraData> mExtraData;
};

void MappedAttrParser::ParseMappedAttrValue(nsAtom* aMappedAttrName,
                                            const nsAString& aMappedAttrValue) {
  // Get the nsCSSPropertyID ID for our mapped attribute.
  nsCSSPropertyID propertyID =
      nsCSSProps::LookupProperty(nsAutoAtomCString(aMappedAttrName));
  if (propertyID != eCSSProperty_UNKNOWN) {
    bool changed = false;  // outparam for ParseProperty.
    NS_ConvertUTF16toUTF8 value(aMappedAttrValue);

    auto* doc = mElement.OwnerDoc();
    changed = Servo_DeclarationBlock_SetPropertyById(
        &EnsureDeclarationBlock(), propertyID, &value, false,
        &EnsureExtraData(), StyleParsingMode::ALLOW_UNITLESS_LENGTH,
        doc->GetCompatibilityMode(), doc->CSSLoader(), StyleCssRuleType::Style,
        {});

    // TODO(emilio): If we want to record these from CSSOM more generally, we
    // can pass the document use counters down the FFI call. For now manually
    // count them.
    if (changed && StaticPrefs::layout_css_use_counters_enabled()) {
      UseCounter useCounter = nsCSSProps::UseCounterFor(propertyID);
      MOZ_ASSERT(useCounter != eUseCounter_UNKNOWN);
      doc->SetUseCounter(useCounter);
    }
    return;
  }
  MOZ_ASSERT(aMappedAttrName == nsGkAtoms::lang,
             "Only 'lang' should be unrecognized!");
  // CSS parser doesn't know about 'lang', so we need to handle it specially.
  if (aMappedAttrName == nsGkAtoms::lang) {
    propertyID = eCSSProperty__x_lang;
    RefPtr<nsAtom> atom = NS_Atomize(aMappedAttrValue);
    Servo_DeclarationBlock_SetIdentStringValue(&EnsureDeclarationBlock(),
                                               propertyID, atom);
  }
}

void MappedAttrParser::TellStyleAlreadyParsedResult(
    nsAtom const* aAtom, SVGAnimatedLength const& aLength) {
  nsCSSPropertyID propertyID =
      nsCSSProps::LookupProperty(nsAutoAtomCString(aAtom));
  SVGElement::UpdateDeclarationBlockFromLength(EnsureDeclarationBlock(),
                                               propertyID, aLength,
                                               SVGElement::ValToUse::Base);
}

void MappedAttrParser::TellStyleAlreadyParsedResult(
    const SVGAnimatedPathSegList& aPath) {
  SVGElement::UpdateDeclarationBlockFromPath(EnsureDeclarationBlock(), aPath,
                                             SVGElement::ValToUse::Base);
}

}  // namespace

//----------------------------------------------------------------------
// Implementation Helpers:

void SVGElement::UpdateMappedDeclarationBlock() {
  MOZ_ASSERT(IsPendingMappedAttributeEvaluation());
  MappedAttrParser mappedAttrParser(*this, mAttrs.GetMappedDeclarationBlock());

  const bool lengthAffectsStyle =
      SVGGeometryProperty::ElementMapsLengthsToStyle(this);

  uint32_t i = 0;
  while (BorrowedAttrInfo info = GetAttrInfoAt(i++)) {
    const nsAttrName* attrName = info.mName;
    if (!attrName->IsAtom() || !IsAttributeMapped(attrName->Atom())) {
      continue;
    }

    if (attrName->Atom() == nsGkAtoms::lang &&
        HasAttr(kNameSpaceID_XML, nsGkAtoms::lang)) {
      // xml:lang has precedence, and will get set via Gecko_GetXMLLangValue().
      continue;
    }

    if (lengthAffectsStyle) {
      auto const* length = GetAnimatedLength(attrName->Atom());

      if (length && length->HasBaseVal()) {
        // This is an element with geometry property set via SVG attribute,
        // and the attribute is already successfully parsed. We want to go
        // through the optimized path to tell the style system the result
        // directly, rather than let it parse the same thing again.
        mappedAttrParser.TellStyleAlreadyParsedResult(attrName->Atom(),
                                                      *length);
        continue;
      }
    }

    if (attrName->Equals(nsGkAtoms::d, kNameSpaceID_None)) {
      const auto* path = GetAnimPathSegList();
      // Note: Only SVGPathElement has d attribute.
      MOZ_ASSERT(
          path,
          "SVGPathElement should have the non-null SVGAnimatedPathSegList");
      // The attribute should have been already successfully parsed.
      // We want to go through the optimized path to tell the style system
      // the result directly, rather than let it parse the same thing again.
      mappedAttrParser.TellStyleAlreadyParsedResult(*path);
      // Some other notes:
      // The syntax of CSS d property is different from SVG d attribute.
      // 1. CSS d proeprty accepts:  none | path(<quoted string>);
      // 2. SVG d attribtue accepts: none | <string>
      // So we cannot use css parser to parse the SVG d attribute directly.
      // Besides, |mAttrs.AttrAt(i)| removes the quotes already, so the svg path
      // in |mAttrs.AttrAt(i)| would be something like `M0,0L1,1z` without the
      // quotes. So css tokenizer cannot recognize this as a quoted string, and
      // so svg_path::SVGPathData::parse() doesn't work for this. Fortunately,
      // we still can rely on the parsed result from
      // SVGElement::ParseAttribute() for d attribute.
      continue;
    }

    nsAutoString value;
    info.mValue->ToString(value);
    mappedAttrParser.ParseMappedAttrValue(attrName->Atom(), value);
  }
  mAttrs.SetMappedDeclarationBlock(mappedAttrParser.TakeDeclarationBlock());
}

/**
 * Helper methods for the type-specific WillChangeXXX methods.
 *
 * This method sends out appropriate pre-change notifications so that selector
 * restyles (e.g. due to changes that cause |elem[attr="val"]| to start/stop
 * matching) work, and it returns an nsAttrValue that _may_ contain the
 * attribute's pre-change value.
 *
 * The nsAttrValue returned by this method depends on whether there are
 * mutation event listeners listening for changes to this element's attributes.
 * If not, then the object returned is empty. If there are, then the
 * nsAttrValue returned contains a serialized copy of the attribute's value
 * prior to the change, and this object should be passed to the corresponding
 * DidChangeXXX method call (assuming a WillChangeXXX call is required for the
 * SVG type - see comment below). This is necessary so that the 'prevValue'
 * property of the mutation event that is dispatched will correctly contain the
 * old value.
 *
 * The reason we need to serialize the old value if there are mutation
 * event listeners is because the underlying nsAttrValue for the attribute
 * points directly to a parsed representation of the attribute (e.g. an
 * SVGAnimatedLengthList*) that is a member of the SVG element. That object
 * will have changed by the time DidChangeXXX has been called, so without the
 * serialization of the old attribute value that we provide, DidChangeXXX
 * would have no way to get the old value to pass to SetAttrAndNotify.
 *
 * We only return the old value when there are mutation event listeners because
 * it's not needed otherwise, and because it's expensive to serialize the old
 * value. This is especially true for list type attributes, which may be built
 * up via the SVG DOM resulting in a large number of Will/DidModifyXXX calls
 * before the script finally finishes setting the attribute.
 *
 * Note that unlike using SetParsedAttr, using Will/DidChangeXXX does NOT check
 * and filter out redundant changes. Before calling WillChangeXXX, the caller
 * should check whether the new and old values are actually the same, and skip
 * calling Will/DidChangeXXX if they are.
 *
 * Also note that not all SVG types use this scheme. For types that can be
 * represented by an nsAttrValue without pointing back to an SVG object (e.g.
 * enums, booleans, integers) we can simply use SetParsedAttr which will do all
 * of the above for us. For such types there is no matching WillChangeXXX
 * method, only DidChangeXXX which calls SetParsedAttr.
 */
nsAttrValue SVGElement::WillChangeValue(
    nsAtom* aName, const mozAutoDocUpdate& aProofOfUpdate) {
  // We need an empty attr value:
  //   a) to pass to BeforeSetAttr when GetParsedAttr returns nullptr
  //   b) to store the old value in the case we have mutation listeners
  //
  // We can use the same value for both purposes, because if GetParsedAttr
  // returns non-null its return value is what will get passed to BeforeSetAttr,
  // not matter what our mutation listener situation is.
  //
  // Also, we should be careful to always return this value to benefit from
  // return value optimization.
  nsAttrValue emptyOrOldAttrValue;
  const nsAttrValue* attrValue = GetParsedAttr(aName);

  // We only need to set the old value if we have listeners since otherwise it
  // isn't used.
  if (attrValue && nsContentUtils::HasMutationListeners(
                       this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED, this)) {
    emptyOrOldAttrValue.SetToSerialized(*attrValue);
  }

  uint8_t modType =
      attrValue ? static_cast<uint8_t>(MutationEvent_Binding::MODIFICATION)
                : static_cast<uint8_t>(MutationEvent_Binding::ADDITION);
  MutationObservers::NotifyAttributeWillChange(this, kNameSpaceID_None, aName,
                                               modType);

  // This is not strictly correct--the attribute value parameter for
  // BeforeSetAttr should reflect the value that *will* be set but that implies
  // allocating, e.g. an extra SVGAnimatedLength, and isn't necessary at the
  // moment since no SVG elements overload BeforeSetAttr. For now we just pass
  // the current value.
  const nsAttrValue* value = attrValue ? attrValue : &emptyOrOldAttrValue;
  BeforeSetAttr(kNameSpaceID_None, aName, value, kNotifyDocumentObservers);
  return emptyOrOldAttrValue;
}

/**
 * Helper methods for the type-specific DidChangeXXX methods.
 *
 * aEmptyOrOldValue will normally be the object returned from the corresponding
 * WillChangeXXX call. This is because:
 * a) WillChangeXXX will ensure the object is set when we have mutation
 *    listeners, and
 * b) WillChangeXXX will ensure the object represents a serialized version of
 *    the old attribute value so that the value doesn't change when the
 *    underlying SVG type is updated.
 *
 * aNewValue is replaced with the old value.
 */
void SVGElement::DidChangeValue(nsAtom* aName,
                                const nsAttrValue& aEmptyOrOldValue,
                                nsAttrValue& aNewValue,
                                const mozAutoDocUpdate& aProofOfUpdate) {
  bool hasListeners = nsContentUtils::HasMutationListeners(
      this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED, this);
  uint8_t modType =
      HasAttr(aName) ? static_cast<uint8_t>(MutationEvent_Binding::MODIFICATION)
                     : static_cast<uint8_t>(MutationEvent_Binding::ADDITION);

  // XXX Really, the fourth argument to SetAttrAndNotify should be null if
  // aEmptyOrOldValue does not represent the actual previous value of the
  // attribute, but currently SVG elements do not even use the old attribute
  // value in |AfterSetAttr|, so this should be ok.
  SetAttrAndNotify(kNameSpaceID_None, aName, nullptr, &aEmptyOrOldValue,
                   aNewValue, nullptr, modType, hasListeners,
                   kNotifyDocumentObservers, kCallAfterSetAttr,
                   GetComposedDoc(), aProofOfUpdate);
}

void SVGElement::MaybeSerializeAttrBeforeRemoval(nsAtom* aName, bool aNotify) {
  if (!aNotify || !nsContentUtils::HasMutationListeners(
                      this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED, this)) {
    return;
  }

  const nsAttrValue* attrValue = mAttrs.GetAttr(aName);
  if (!attrValue) return;

  nsAutoString serializedValue;
  attrValue->ToString(serializedValue);
  nsAttrValue oldAttrValue(serializedValue);
  bool oldValueSet;
  mAttrs.SetAndSwapAttr(aName, oldAttrValue, &oldValueSet);
}

nsAtom* SVGElement::GetEventNameForAttr(nsAtom* aAttr) {
  if (IsSVGElement(nsGkAtoms::svg)) {
    if (aAttr == nsGkAtoms::onload) return nsGkAtoms::onSVGLoad;
    if (aAttr == nsGkAtoms::onscroll) return nsGkAtoms::onSVGScroll;
  }
  if (aAttr == nsGkAtoms::onbegin) return nsGkAtoms::onbeginEvent;
  if (aAttr == nsGkAtoms::onrepeat) return nsGkAtoms::onrepeatEvent;
  if (aAttr == nsGkAtoms::onend) return nsGkAtoms::onendEvent;

  return SVGElementBase::GetEventNameForAttr(aAttr);
}

SVGViewportElement* SVGElement::GetCtx() const {
  return SVGContentUtils::GetNearestViewportElement(this);
}

/* virtual */
gfxMatrix SVGElement::PrependLocalTransformsTo(const gfxMatrix& aMatrix,
                                               SVGTransformTypes aWhich) const {
  return aMatrix;
}

SVGElement::LengthAttributesInfo SVGElement::GetLengthInfo() {
  return LengthAttributesInfo(nullptr, nullptr, 0);
}

void SVGElement::SetLength(nsAtom* aName, const SVGAnimatedLength& aLength) {
  LengthAttributesInfo lengthInfo = GetLengthInfo();

  for (uint32_t i = 0; i < lengthInfo.mCount; i++) {
    if (aName == lengthInfo.mInfos[i].mName) {
      lengthInfo.mValues[i] = aLength;
      DidAnimateLength(i);
      return;
    }
  }
  MOZ_ASSERT(false, "no length found to set");
}

nsAttrValue SVGElement::WillChangeLength(
    uint8_t aAttrEnum, const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(GetLengthInfo().mInfos[aAttrEnum].mName,
                         aProofOfUpdate);
}

void SVGElement::DidChangeLength(uint8_t aAttrEnum,
                                 const nsAttrValue& aEmptyOrOldValue,
                                 const mozAutoDocUpdate& aProofOfUpdate) {
  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangeLength on element with no length attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mValues[aAttrEnum], nullptr);

  DidChangeValue(info.mInfos[aAttrEnum].mName, aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

void SVGElement::DidAnimateLength(uint8_t aAttrEnum) {
  // We need to do this here. Normally the SMIL restyle would also cause us to
  // do this from DidSetComputedStyle, but we don't have that guarantee if our
  // frame gets reconstructed.
  ClearAnyCachedPath();

  if (SVGGeometryProperty::ElementMapsLengthsToStyle(this)) {
    nsCSSPropertyID propId =
        SVGGeometryProperty::AttrEnumToCSSPropId(this, aAttrEnum);

    // We don't map use element width/height currently. We can remove this
    // test when we do.
    if (propId != eCSSProperty_UNKNOWN) {
      auto lengthInfo = GetLengthInfo();
      if (lengthInfo.mValues[aAttrEnum].IsAnimated()) {
        SMILOverrideStyle()->SetSMILValue(propId,
                                          lengthInfo.mValues[aAttrEnum]);
      } else {
        SMILOverrideStyle()->ClearSMILValue(propId);
      }
    }
  }

  auto info = GetLengthInfo();
  DidAnimateAttribute(kNameSpaceID_None, info.mInfos[aAttrEnum].mName);
}

SVGAnimatedLength* SVGElement::GetAnimatedLength(uint8_t aAttrEnum) {
  LengthAttributesInfo info = GetLengthInfo();
  if (aAttrEnum < info.mCount) {
    return &info.mValues[aAttrEnum];
  }
  MOZ_ASSERT_UNREACHABLE("Bad attrEnum");
  return nullptr;
}

SVGAnimatedLength* SVGElement::GetAnimatedLength(const nsAtom* aAttrName) {
  LengthAttributesInfo lengthInfo = GetLengthInfo();

  for (uint32_t i = 0; i < lengthInfo.mCount; i++) {
    if (aAttrName == lengthInfo.mInfos[i].mName) {
      return &lengthInfo.mValues[i];
    }
  }
  return nullptr;
}

void SVGElement::GetAnimatedLengthValues(float* aFirst, ...) {
  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mCount > 0,
               "GetAnimatedLengthValues on element with no length attribs");

  SVGElementMetrics metrics(this);

  float* f = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (f && i < info.mCount) {
    *f = info.mValues[i++].GetAnimValue(metrics);
    f = va_arg(args, float*);
  }

  va_end(args);
}

SVGElement::LengthListAttributesInfo SVGElement::GetLengthListInfo() {
  return LengthListAttributesInfo(nullptr, nullptr, 0);
}

nsAttrValue SVGElement::WillChangeLengthList(
    uint8_t aAttrEnum, const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(GetLengthListInfo().mInfos[aAttrEnum].mName,
                         aProofOfUpdate);
}

void SVGElement::DidChangeLengthList(uint8_t aAttrEnum,
                                     const nsAttrValue& aEmptyOrOldValue,
                                     const mozAutoDocUpdate& aProofOfUpdate) {
  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangeLengthList on element with no length list attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mValues[aAttrEnum].GetBaseValue(), nullptr);

  DidChangeValue(info.mInfos[aAttrEnum].mName, aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

void SVGElement::GetAnimatedLengthListValues(SVGUserUnitList* aFirst, ...) {
  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(
      info.mCount > 0,
      "GetAnimatedLengthListValues on element with no length list attribs");

  SVGUserUnitList* list = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (list && i < info.mCount) {
    list->Init(&(info.mValues[i].GetAnimValue()), this, info.mInfos[i].mAxis);
    ++i;
    list = va_arg(args, SVGUserUnitList*);
  }

  va_end(args);
}

SVGAnimatedLengthList* SVGElement::GetAnimatedLengthList(uint8_t aAttrEnum) {
  LengthListAttributesInfo info = GetLengthListInfo();
  if (aAttrEnum < info.mCount) {
    return &(info.mValues[aAttrEnum]);
  }
  MOZ_ASSERT_UNREACHABLE("Bad attrEnum");
  return nullptr;
}

SVGElement::NumberListAttributesInfo SVGElement::GetNumberListInfo() {
  return NumberListAttributesInfo(nullptr, nullptr, 0);
}

nsAttrValue SVGElement::WillChangeNumberList(
    uint8_t aAttrEnum, const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(GetNumberListInfo().mInfos[aAttrEnum].mName,
                         aProofOfUpdate);
}

void SVGElement::DidChangeNumberList(uint8_t aAttrEnum,
                                     const nsAttrValue& aEmptyOrOldValue,
                                     const mozAutoDocUpdate& aProofOfUpdate) {
  NumberListAttributesInfo info = GetNumberListInfo();

  MOZ_ASSERT(info.mCount > 0,
             "DidChangeNumberList on element with no number list attribs");
  MOZ_ASSERT(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mValues[aAttrEnum].GetBaseValue(), nullptr);

  DidChangeValue(info.mInfos[aAttrEnum].mName, aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

SVGAnimatedNumberList* SVGElement::GetAnimatedNumberList(uint8_t aAttrEnum) {
  NumberListAttributesInfo info = GetNumberListInfo();
  if (aAttrEnum < info.mCount) {
    return &(info.mValues[aAttrEnum]);
  }
  MOZ_ASSERT(false, "Bad attrEnum");
  return nullptr;
}

SVGAnimatedNumberList* SVGElement::GetAnimatedNumberList(nsAtom* aAttrName) {
  NumberListAttributesInfo info = GetNumberListInfo();
  for (uint32_t i = 0; i < info.mCount; i++) {
    if (aAttrName == info.mInfos[i].mName) {
      return &info.mValues[i];
    }
  }
  MOZ_ASSERT(false, "Bad caller");
  return nullptr;
}

nsAttrValue SVGElement::WillChangePointList(
    const mozAutoDocUpdate& aProofOfUpdate) {
  MOZ_ASSERT(GetPointListAttrName(), "Changing non-existent point list?");
  return WillChangeValue(GetPointListAttrName(), aProofOfUpdate);
}

void SVGElement::DidChangePointList(const nsAttrValue& aEmptyOrOldValue,
                                    const mozAutoDocUpdate& aProofOfUpdate) {
  MOZ_ASSERT(GetPointListAttrName(), "Changing non-existent point list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimatedPointList()->GetBaseValue(), nullptr);

  DidChangeValue(GetPointListAttrName(), aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

void SVGElement::DidAnimatePointList() {
  MOZ_ASSERT(GetPointListAttrName(), "Animating non-existent path data?");

  ClearAnyCachedPath();

  DidAnimateAttribute(kNameSpaceID_None, GetPointListAttrName());
}

nsAttrValue SVGElement::WillChangePathSegList(
    const mozAutoDocUpdate& aProofOfUpdate) {
  MOZ_ASSERT(GetPathDataAttrName(), "Changing non-existent path seg list?");
  return WillChangeValue(GetPathDataAttrName(), aProofOfUpdate);
}

void SVGElement::DidChangePathSegList(const nsAttrValue& aEmptyOrOldValue,
                                      const mozAutoDocUpdate& aProofOfUpdate) {
  MOZ_ASSERT(GetPathDataAttrName(), "Changing non-existent path seg list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimPathSegList()->GetBaseValue(), nullptr);

  DidChangeValue(GetPathDataAttrName(), aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

void SVGElement::DidAnimatePathSegList() {
  nsStaticAtom* name = GetPathDataAttrName();
  MOZ_ASSERT(name, "Animating non-existent path data?");

  ClearAnyCachedPath();

  // Notify style we have to update the d property because of SMIL animation.
  if (name == nsGkAtoms::d) {
    auto* animPathSegList = GetAnimPathSegList();
    if (animPathSegList->IsAnimating()) {
      SMILOverrideStyle()->SetSMILValue(nsCSSPropertyID::eCSSProperty_d,
                                        *animPathSegList);
    } else {
      SMILOverrideStyle()->ClearSMILValue(nsCSSPropertyID::eCSSProperty_d);
    }
  }

  DidAnimateAttribute(kNameSpaceID_None, name);
}

SVGElement::NumberAttributesInfo SVGElement::GetNumberInfo() {
  return NumberAttributesInfo(nullptr, nullptr, 0);
}

void SVGElement::DidChangeNumber(uint8_t aAttrEnum) {
  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangeNumber on element with no number attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue attrValue;
  attrValue.SetTo(info.mValues[aAttrEnum].GetBaseValue(), nullptr);

  SetParsedAttr(kNameSpaceID_None, info.mInfos[aAttrEnum].mName, nullptr,
                attrValue, true);
}

void SVGElement::GetAnimatedNumberValues(float* aFirst, ...) {
  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mCount > 0,
               "GetAnimatedNumberValues on element with no number attribs");

  float* f = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (f && i < info.mCount) {
    *f = info.mValues[i++].GetAnimValue();
    f = va_arg(args, float*);
  }
  va_end(args);
}

SVGElement::NumberPairAttributesInfo SVGElement::GetNumberPairInfo() {
  return NumberPairAttributesInfo(nullptr, nullptr, 0);
}

nsAttrValue SVGElement::WillChangeNumberPair(uint8_t aAttrEnum) {
  mozAutoDocUpdate updateBatch(GetComposedDoc(), kDontNotifyDocumentObservers);
  return WillChangeValue(GetNumberPairInfo().mInfos[aAttrEnum].mName,
                         updateBatch);
}

void SVGElement::DidChangeNumberPair(uint8_t aAttrEnum,
                                     const nsAttrValue& aEmptyOrOldValue) {
  NumberPairAttributesInfo info = GetNumberPairInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangePairNumber on element with no number pair attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mValues[aAttrEnum], nullptr);

  mozAutoDocUpdate updateBatch(GetComposedDoc(), kNotifyDocumentObservers);
  DidChangeValue(info.mInfos[aAttrEnum].mName, aEmptyOrOldValue, newValue,
                 updateBatch);
}

SVGElement::IntegerAttributesInfo SVGElement::GetIntegerInfo() {
  return IntegerAttributesInfo(nullptr, nullptr, 0);
}

void SVGElement::DidChangeInteger(uint8_t aAttrEnum) {
  IntegerAttributesInfo info = GetIntegerInfo();
  NS_ASSERTION(info.mCount > 0,
               "DidChangeInteger on element with no integer attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue attrValue;
  attrValue.SetTo(info.mValues[aAttrEnum].GetBaseValue(), nullptr);

  SetParsedAttr(kNameSpaceID_None, info.mInfos[aAttrEnum].mName, nullptr,
                attrValue, true);
}

void SVGElement::GetAnimatedIntegerValues(int32_t* aFirst, ...) {
  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mCount > 0,
               "GetAnimatedIntegerValues on element with no integer attribs");

  int32_t* n = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (n && i < info.mCount) {
    *n = info.mValues[i++].GetAnimValue();
    n = va_arg(args, int32_t*);
  }
  va_end(args);
}

SVGElement::IntegerPairAttributesInfo SVGElement::GetIntegerPairInfo() {
  return IntegerPairAttributesInfo(nullptr, nullptr, 0);
}

nsAttrValue SVGElement::WillChangeIntegerPair(
    uint8_t aAttrEnum, const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(GetIntegerPairInfo().mInfos[aAttrEnum].mName,
                         aProofOfUpdate);
}

void SVGElement::DidChangeIntegerPair(uint8_t aAttrEnum,
                                      const nsAttrValue& aEmptyOrOldValue,
                                      const mozAutoDocUpdate& aProofOfUpdate) {
  IntegerPairAttributesInfo info = GetIntegerPairInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangeIntegerPair on element with no integer pair attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mValues[aAttrEnum], nullptr);

  DidChangeValue(info.mInfos[aAttrEnum].mName, aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

SVGElement::BooleanAttributesInfo SVGElement::GetBooleanInfo() {
  return BooleanAttributesInfo(nullptr, nullptr, 0);
}

void SVGElement::DidChangeBoolean(uint8_t aAttrEnum) {
  BooleanAttributesInfo info = GetBooleanInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangeBoolean on element with no boolean attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue attrValue(info.mValues[aAttrEnum].GetBaseValueAtom());
  SetParsedAttr(kNameSpaceID_None, info.mInfos[aAttrEnum].mName, nullptr,
                attrValue, true);
}

SVGElement::EnumAttributesInfo SVGElement::GetEnumInfo() {
  return EnumAttributesInfo(nullptr, nullptr, 0);
}

void SVGElement::DidChangeEnum(uint8_t aAttrEnum) {
  EnumAttributesInfo info = GetEnumInfo();

  NS_ASSERTION(info.mCount > 0,
               "DidChangeEnum on element with no enum attribs");
  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  nsAttrValue attrValue(info.mValues[aAttrEnum].GetBaseValueAtom(this));
  SetParsedAttr(kNameSpaceID_None, info.mInfos[aAttrEnum].mName, nullptr,
                attrValue, true);
}

SVGAnimatedOrient* SVGElement::GetAnimatedOrient() { return nullptr; }

nsAttrValue SVGElement::WillChangeOrient(
    const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(nsGkAtoms::orient, aProofOfUpdate);
}

void SVGElement::DidChangeOrient(const nsAttrValue& aEmptyOrOldValue,
                                 const mozAutoDocUpdate& aProofOfUpdate) {
  SVGAnimatedOrient* orient = GetAnimatedOrient();

  NS_ASSERTION(orient, "DidChangeOrient on element with no orient attrib");

  nsAttrValue newValue;
  newValue.SetTo(*orient, nullptr);

  DidChangeValue(nsGkAtoms::orient, aEmptyOrOldValue, newValue, aProofOfUpdate);
}

SVGAnimatedViewBox* SVGElement::GetAnimatedViewBox() { return nullptr; }

nsAttrValue SVGElement::WillChangeViewBox(
    const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(nsGkAtoms::viewBox, aProofOfUpdate);
}

void SVGElement::DidChangeViewBox(const nsAttrValue& aEmptyOrOldValue,
                                  const mozAutoDocUpdate& aProofOfUpdate) {
  SVGAnimatedViewBox* viewBox = GetAnimatedViewBox();

  NS_ASSERTION(viewBox, "DidChangeViewBox on element with no viewBox attrib");

  nsAttrValue newValue;
  newValue.SetTo(*viewBox, nullptr);

  DidChangeValue(nsGkAtoms::viewBox, aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

SVGAnimatedPreserveAspectRatio* SVGElement::GetAnimatedPreserveAspectRatio() {
  return nullptr;
}

nsAttrValue SVGElement::WillChangePreserveAspectRatio(
    const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(nsGkAtoms::preserveAspectRatio, aProofOfUpdate);
}

void SVGElement::DidChangePreserveAspectRatio(
    const nsAttrValue& aEmptyOrOldValue,
    const mozAutoDocUpdate& aProofOfUpdate) {
  SVGAnimatedPreserveAspectRatio* preserveAspectRatio =
      GetAnimatedPreserveAspectRatio();

  NS_ASSERTION(preserveAspectRatio,
               "DidChangePreserveAspectRatio on element with no "
               "preserveAspectRatio attrib");

  nsAttrValue newValue;
  newValue.SetTo(*preserveAspectRatio, nullptr);

  DidChangeValue(nsGkAtoms::preserveAspectRatio, aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

nsAttrValue SVGElement::WillChangeTransformList(
    const mozAutoDocUpdate& aProofOfUpdate) {
  return WillChangeValue(GetTransformListAttrName(), aProofOfUpdate);
}

void SVGElement::DidChangeTransformList(
    const nsAttrValue& aEmptyOrOldValue,
    const mozAutoDocUpdate& aProofOfUpdate) {
  MOZ_ASSERT(GetTransformListAttrName(),
             "Changing non-existent transform list?");

  // The transform attribute is being set, so we must ensure that the
  // SVGAnimatedTransformList is/has been allocated:
  nsAttrValue newValue;
  newValue.SetTo(GetAnimatedTransformList(DO_ALLOCATE)->GetBaseValue(),
                 nullptr);

  DidChangeValue(GetTransformListAttrName(), aEmptyOrOldValue, newValue,
                 aProofOfUpdate);
}

void SVGElement::DidAnimateTransformList(int32_t aModType) {
  MOZ_ASSERT(GetTransformListAttrName(),
             "Animating non-existent transform data?");

  if (auto* frame = GetPrimaryFrame()) {
    nsAtom* transformAttr = GetTransformListAttrName();
    frame->AttributeChanged(kNameSpaceID_None, transformAttr, aModType);
    // When script changes the 'transform' attribute, Element::SetAttrAndNotify
    // will call MutationObservers::NotifyAttributeChanged, under which
    // SVGTransformableElement::GetAttributeChangeHint will be called and an
    // appropriate change event posted to update our frame's overflow rects.
    // The SetAttrAndNotify doesn't happen for transform changes caused by
    // 'animateTransform' though (and sending out the mutation events that
    // MutationObservers::NotifyAttributeChanged dispatches would be
    // inappropriate anyway), so we need to post the change event ourself.
    nsChangeHint changeHint = GetAttributeChangeHint(transformAttr, aModType);
    if (changeHint) {
      nsLayoutUtils::PostRestyleEvent(this, RestyleHint{0}, changeHint);
    }
    SVGObserverUtils::InvalidateRenderingObservers(frame);
    return;
  }
  SVGObserverUtils::InvalidateDirectRenderingObservers(this);
}

SVGElement::StringAttributesInfo SVGElement::GetStringInfo() {
  return StringAttributesInfo(nullptr, nullptr, 0);
}

void SVGElement::GetStringBaseValue(uint8_t aAttrEnum,
                                    nsAString& aResult) const {
  SVGElement::StringAttributesInfo info =
      const_cast<SVGElement*>(this)->GetStringInfo();

  NS_ASSERTION(info.mCount > 0,
               "GetBaseValue on element with no string attribs");

  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  GetAttr(info.mInfos[aAttrEnum].mNamespaceID, info.mInfos[aAttrEnum].mName,
          aResult);
}

void SVGElement::SetStringBaseValue(uint8_t aAttrEnum,
                                    const nsAString& aValue) {
  SVGElement::StringAttributesInfo info = GetStringInfo();

  NS_ASSERTION(info.mCount > 0,
               "SetBaseValue on element with no string attribs");

  NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

  SetAttr(info.mInfos[aAttrEnum].mNamespaceID, info.mInfos[aAttrEnum].mName,
          aValue, true);
}

SVGElement::StringListAttributesInfo SVGElement::GetStringListInfo() {
  return StringListAttributesInfo(nullptr, nullptr, 0);
}

nsAttrValue SVGElement::WillChangeStringList(
    bool aIsConditionalProcessingAttribute, uint8_t aAttrEnum,
    const mozAutoDocUpdate& aProofOfUpdate) {
  nsStaticAtom* name;
  if (aIsConditionalProcessingAttribute) {
    nsCOMPtr<SVGTests> tests(do_QueryInterface(this));
    name = tests->GetAttrName(aAttrEnum);
  } else {
    name = GetStringListInfo().mInfos[aAttrEnum].mName;
  }
  return WillChangeValue(name, aProofOfUpdate);
}

void SVGElement::DidChangeStringList(bool aIsConditionalProcessingAttribute,
                                     uint8_t aAttrEnum,
                                     const nsAttrValue& aEmptyOrOldValue,
                                     const mozAutoDocUpdate& aProofOfUpdate) {
  nsStaticAtom* name;
  nsAttrValue newValue;
  nsCOMPtr<SVGTests> tests;

  if (aIsConditionalProcessingAttribute) {
    tests = do_QueryObject(this);
    name = tests->GetAttrName(aAttrEnum);
    tests->GetAttrValue(aAttrEnum, newValue);
  } else {
    StringListAttributesInfo info = GetStringListInfo();

    NS_ASSERTION(info.mCount > 0,
                 "DidChangeStringList on element with no string list attribs");
    NS_ASSERTION(aAttrEnum < info.mCount, "aAttrEnum out of range");

    name = info.mInfos[aAttrEnum].mName;
    newValue.SetTo(info.mValues[aAttrEnum], nullptr);
  }

  DidChangeValue(name, aEmptyOrOldValue, newValue, aProofOfUpdate);

  if (aIsConditionalProcessingAttribute) {
    tests->MaybeInvalidate();
  }
}

void SVGElement::DidAnimateAttribute(int32_t aNameSpaceID, nsAtom* aAttribute) {
  if (auto* frame = GetPrimaryFrame()) {
    frame->AttributeChanged(aNameSpaceID, aAttribute,
                            MutationEvent_Binding::MODIFICATION);
    SVGObserverUtils::InvalidateRenderingObservers(frame);
    return;
  }
  SVGObserverUtils::InvalidateDirectRenderingObservers(this);
}

nsresult SVGElement::ReportAttributeParseFailure(Document* aDocument,
                                                 nsAtom* aAttribute,
                                                 const nsAString& aValue) {
  AutoTArray<nsString, 2> strings;
  strings.AppendElement(nsDependentAtomString(aAttribute));
  strings.AppendElement(aValue);
  return SVGContentUtils::ReportToConsole(aDocument, "AttributeParseWarning",
                                          strings);
}

UniquePtr<SMILAttr> SVGElement::GetAnimatedAttr(int32_t aNamespaceID,
                                                nsAtom* aName) {
  if (aNamespaceID == kNameSpaceID_None) {
    // Transforms:
    if (GetTransformListAttrName() == aName) {
      // The transform attribute is being animated, so we must ensure that the
      // SVGAnimatedTransformList is/has been allocated:
      return GetAnimatedTransformList(DO_ALLOCATE)->ToSMILAttr(this);
    }

    // Motion (fake 'attribute' for animateMotion)
    if (aName == nsGkAtoms::mozAnimateMotionDummyAttr) {
      return MakeUnique<SVGMotionSMILAttr>(this);
    }

    // Lengths:
    LengthAttributesInfo info = GetLengthInfo();
    for (uint32_t i = 0; i < info.mCount; i++) {
      if (aName == info.mInfos[i].mName) {
        return info.mValues[i].ToSMILAttr(this);
      }
    }

    // Numbers:
    {
      NumberAttributesInfo info = GetNumberInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          return info.mValues[i].ToSMILAttr(this);
        }
      }
    }

    // Number Pairs:
    {
      NumberPairAttributesInfo info = GetNumberPairInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          return info.mValues[i].ToSMILAttr(this);
        }
      }
    }

    // Integers:
    {
      IntegerAttributesInfo info = GetIntegerInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          return info.mValues[i].ToSMILAttr(this);
        }
      }
    }

    // Integer Pairs:
    {
      IntegerPairAttributesInfo info = GetIntegerPairInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          return info.mValues[i].ToSMILAttr(this);
        }
      }
    }

    // Enumerations:
    {
      EnumAttributesInfo info = GetEnumInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          return info.mValues[i].ToSMILAttr(this);
        }
      }
    }

    // Booleans:
    {
      BooleanAttributesInfo info = GetBooleanInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          return info.mValues[i].ToSMILAttr(this);
        }
      }
    }

    // orient:
    if (aName == nsGkAtoms::orient) {
      SVGAnimatedOrient* orient = GetAnimatedOrient();
      return orient ? orient->ToSMILAttr(this) : nullptr;
    }

    // viewBox:
    if (aName == nsGkAtoms::viewBox) {
      SVGAnimatedViewBox* viewBox = GetAnimatedViewBox();
      return viewBox ? viewBox->ToSMILAttr(this) : nullptr;
    }

    // preserveAspectRatio:
    if (aName == nsGkAtoms::preserveAspectRatio) {
      SVGAnimatedPreserveAspectRatio* preserveAspectRatio =
          GetAnimatedPreserveAspectRatio();
      return preserveAspectRatio ? preserveAspectRatio->ToSMILAttr(this)
                                 : nullptr;
    }

    // NumberLists:
    {
      NumberListAttributesInfo info = GetNumberListInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          MOZ_ASSERT(i <= UCHAR_MAX, "Too many attributes");
          return info.mValues[i].ToSMILAttr(this, uint8_t(i));
        }
      }
    }

    // LengthLists:
    {
      LengthListAttributesInfo info = GetLengthListInfo();
      for (uint32_t i = 0; i < info.mCount; i++) {
        if (aName == info.mInfos[i].mName) {
          MOZ_ASSERT(i <= UCHAR_MAX, "Too many attributes");
          return info.mValues[i].ToSMILAttr(this, uint8_t(i),
                                            info.mInfos[i].mAxis,
                                            info.mInfos[i].mCouldZeroPadList);
        }
      }
    }

    // PointLists:
    {
      if (GetPointListAttrName() == aName) {
        SVGAnimatedPointList* pointList = GetAnimatedPointList();
        if (pointList) {
          return pointList->ToSMILAttr(this);
        }
      }
    }

    // PathSegLists:
    {
      if (GetPathDataAttrName() == aName) {
        SVGAnimatedPathSegList* segList = GetAnimPathSegList();
        if (segList) {
          return segList->ToSMILAttr(this);
        }
      }
    }

    if (aName == nsGkAtoms::_class) {
      return mClassAttribute.ToSMILAttr(this);
    }
  }

  // Strings
  {
    StringAttributesInfo info = GetStringInfo();
    for (uint32_t i = 0; i < info.mCount; i++) {
      if (aNamespaceID == info.mInfos[i].mNamespaceID &&
          aName == info.mInfos[i].mName) {
        return info.mValues[i].ToSMILAttr(this);
      }
    }
  }

  return nullptr;
}

void SVGElement::AnimationNeedsResample() {
  Document* doc = GetComposedDoc();
  if (doc && doc->HasAnimationController()) {
    doc->GetAnimationController()->SetResampleNeeded();
  }
}

void SVGElement::FlushAnimations() {
  Document* doc = GetComposedDoc();
  if (doc && doc->HasAnimationController()) {
    doc->GetAnimationController()->FlushResampleRequests();
  }
}

void SVGElement::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                        size_t* aNodeSize) const {
  Element::AddSizeOfExcludingThis(aSizes, aNodeSize);
}

}  // namespace mozilla::dom
