/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Unused.h"

#include "nsSVGElement.h"

#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGTests.h"
#include "nsContentUtils.h"
#include "nsICSSDeclaration.h"
#include "nsIDocument.h"
#include "nsIDOMMutationEvent.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozAutoDocUpdate.h"
#include "nsError.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsRuleWalker.h"
#include "mozilla/css/Declaration.h"
#include "nsCSSProps.h"
#include "nsCSSParser.h"
#include "mozilla/EventListenerManager.h"
#include "nsLayoutUtils.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGLength2.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsSVGInteger.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGAngle.h"
#include "nsSVGBoolean.h"
#include "nsSVGEnum.h"
#include "nsSVGViewBox.h"
#include "nsSVGString.h"
#include "mozilla/dom/SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedPointList.h"
#include "SVGAnimatedPathSegList.h"
#include "SVGContentUtils.h"
#include "SVGGeometryElement.h"
#include "nsIFrame.h"
#include "nsQueryObject.h"
#include <stdarg.h>
#include "SVGMotionSMILAttr.h"
#include "nsAttrValueOrString.h"
#include "nsSMILAnimationController.h"
#include "mozilla/dom/SVGElementBinding.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/Unused.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

// This is needed to ensure correct handling of calls to the
// vararg-list methods in this file:
//   nsSVGElement::GetAnimated{Length,Number,Integer}Values
// See bug 547964 for details:
static_assert(sizeof(void*) == sizeof(nullptr),
              "nullptr should be the correct size");

nsresult
NS_NewSVGElement(Element **aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
{
  RefPtr<nsSVGElement> it = new nsSVGElement(aNodeInfo);
  nsresult rv = it->Init();

  if (NS_FAILED(rv)) {
    return rv;
  }

  it.forget(aResult);
  return rv;
}

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGElement)

nsSVGEnumMapping nsSVGElement::sSVGUnitTypesMap[] = {
  {&nsGkAtoms::userSpaceOnUse, SVG_UNIT_TYPE_USERSPACEONUSE},
  {&nsGkAtoms::objectBoundingBox, SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
  {nullptr, 0}
};

nsSVGElement::nsSVGElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsSVGElementBase(aNodeInfo)
{
}

nsSVGElement::~nsSVGElement()
{
  OwnerDoc()->UnscheduleSVGForPresAttrEvaluation(this);
}

JSObject*
nsSVGElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGElementBinding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsSVGElement::GetSVGClassName(nsISupports** aClassName)
{
  *aClassName = ClassName().take();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  NS_ADDREF(*aStyle = Style());
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGElement::DidAnimateClass()
{
  nsAutoString src;
  mClassAttribute.GetAnimValue(src, this);
  if (!mClassAnimAttr) {
    mClassAnimAttr = new nsAttrValue();
  }
  mClassAnimAttr->ParseAtomArray(src);

  nsIPresShell* shell = OwnerDoc()->GetShell();
  if (shell) {
    shell->RestyleForAnimation(this, eRestyle_Self);
  }
}

nsresult
nsSVGElement::Init()
{
  // Set up length attributes - can't do this in the constructor
  // because we can't do a virtual call at that point

  LengthAttributesInfo lengthInfo = GetLengthInfo();

  uint32_t i;
  for (i = 0; i < lengthInfo.mLengthCount; i++) {
    lengthInfo.Reset(i);
  }

  NumberAttributesInfo numberInfo = GetNumberInfo();

  for (i = 0; i < numberInfo.mNumberCount; i++) {
    numberInfo.Reset(i);
  }

  NumberPairAttributesInfo numberPairInfo = GetNumberPairInfo();

  for (i = 0; i < numberPairInfo.mNumberPairCount; i++) {
    numberPairInfo.Reset(i);
  }

  IntegerAttributesInfo integerInfo = GetIntegerInfo();

  for (i = 0; i < integerInfo.mIntegerCount; i++) {
    integerInfo.Reset(i);
  }

  IntegerPairAttributesInfo integerPairInfo = GetIntegerPairInfo();

  for (i = 0; i < integerPairInfo.mIntegerPairCount; i++) {
    integerPairInfo.Reset(i);
  }

  AngleAttributesInfo angleInfo = GetAngleInfo();

  for (i = 0; i < angleInfo.mAngleCount; i++) {
    angleInfo.Reset(i);
  }

  BooleanAttributesInfo booleanInfo = GetBooleanInfo();

  for (i = 0; i < booleanInfo.mBooleanCount; i++) {
    booleanInfo.Reset(i);
  }

  EnumAttributesInfo enumInfo = GetEnumInfo();

  for (i = 0; i < enumInfo.mEnumCount; i++) {
    enumInfo.Reset(i);
  }

  nsSVGViewBox *viewBox = GetViewBox();

  if (viewBox) {
    viewBox->Init();
  }

  SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
    GetPreserveAspectRatio();

  if (preserveAspectRatio) {
    preserveAspectRatio->Init();
  }

  LengthListAttributesInfo lengthListInfo = GetLengthListInfo();

  for (i = 0; i < lengthListInfo.mLengthListCount; i++) {
    lengthListInfo.Reset(i);
  }

  NumberListAttributesInfo numberListInfo = GetNumberListInfo();

  for (i = 0; i < numberListInfo.mNumberListCount; i++) {
    numberListInfo.Reset(i);
  }

  // No need to reset SVGPointList since the default value is always the same
  // (an empty list).

  // No need to reset SVGPathData since the default value is always the same
  // (an empty list).

  StringAttributesInfo stringInfo = GetStringInfo();

  for (i = 0; i < stringInfo.mStringCount; i++) {
    stringInfo.Reset(i);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED(nsSVGElement, nsSVGElementBase,
                            nsIDOMNode, nsIDOMElement,
                            nsIDOMSVGElement)

//----------------------------------------------------------------------
// Implementation

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                         nsIContent* aBindingParent,
                         bool aCompileEventHandlers)
{
  nsresult rv = nsSVGElementBase::BindToTree(aDocument, aParent,
                                             aBindingParent,
                                             aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!MayHaveStyle()) {
    return NS_OK;
  }
  const nsAttrValue* oldVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);

  if (oldVal && oldVal->Type() == nsAttrValue::eCSSDeclaration) {
    // we need to force a reparse because the baseURI of the document
    // may have changed, and in particular because we may be clones of
    // XBL anonymous content now being bound to the document we should
    // render in and due to the hacky way in which we implement the
    // interaction of XBL and SVG resources.  Once we have a sane
    // ownerDocument on XBL anonymous content, this can all go away.
    nsAttrValue attrValue;
    nsAutoString stringValue;
    oldVal->ToString(stringValue);
    // Force in data doc, since we already have a style rule
    ParseStyleAttribute(stringValue, attrValue, true);
    // Don't bother going through SetInlineStyleDeclaration; we don't
    // want to fire off mutation events or document notifications anyway
    bool oldValueSet;
    rv = mAttrsAndChildren.SetAndSwapAttr(nsGkAtoms::style, attrValue,
                                          &oldValueSet);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsSVGElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                           const nsAttrValue* aValue,
                           const nsAttrValue* aOldValue, bool aNotify)
{
  // We don't currently use nsMappedAttributes within SVG. If this changes, we
  // need to be very careful because some nsAttrValues used by SVG point to
  // member data of SVG elements and if an nsAttrValue outlives the SVG element
  // whose data it points to (by virtue of being stored in
  // mAttrsAndChildren->mMappedAttributes, meaning it's shared between
  // elements), the pointer will dangle. See bug 724680.
  MOZ_ASSERT(!mAttrsAndChildren.HasMappedAttrs(),
             "Unexpected use of nsMappedAttributes within SVG");

  // If this is an svg presentation attribute we need to map it into
  // the content declaration block.
  // XXX For some reason incremental mapping doesn't work, so for now
  // just delete the style rule and lazily reconstruct it as needed).
  if (aNamespaceID == kNameSpaceID_None && IsAttributeMapped(aName)) {
    mContentDeclarationBlock = nullptr;
    if (OwnerDoc()->GetStyleBackendType() == StyleBackendType::Servo) {
      OwnerDoc()->ScheduleSVGForPresAttrEvaluation(this);
    }
  }

  if (IsEventAttributeName(aName) && aValue) {
    MOZ_ASSERT(aValue->Type() == nsAttrValue::eString,
               "Expected string value for script body");
    nsresult rv = SetEventHandler(GetEventNameForAttr(aName),
                                  aValue->GetStringValue());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nsSVGElementBase::AfterSetAttr(aNamespaceID, aName, aValue, aOldValue,
                                        aNotify);
}

bool
nsSVGElement::ParseAttribute(int32_t aNamespaceID,
                             nsIAtom* aAttribute,
                             const nsAString& aValue,
                             nsAttrValue& aResult)
{
  nsresult rv = NS_OK;
  bool foundMatch = false;
  bool didSetResult = false;

  if (aNamespaceID == kNameSpaceID_None) {
    // Check for nsSVGLength2 attribute
    LengthAttributesInfo lengthInfo = GetLengthInfo();

    uint32_t i;
    for (i = 0; i < lengthInfo.mLengthCount; i++) {
      if (aAttribute == *lengthInfo.mLengthInfo[i].mName) {
        rv = lengthInfo.mLengths[i].SetBaseValueString(aValue, this, false);
        if (NS_FAILED(rv)) {
          lengthInfo.Reset(i);
        } else {
          aResult.SetTo(lengthInfo.mLengths[i], &aValue);
          didSetResult = true;
        }
        foundMatch = true;
        break;
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedLengthList attribute
      LengthListAttributesInfo lengthListInfo = GetLengthListInfo();
      for (i = 0; i < lengthListInfo.mLengthListCount; i++) {
        if (aAttribute == *lengthListInfo.mLengthListInfo[i].mName) {
          rv = lengthListInfo.mLengthLists[i].SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            lengthListInfo.Reset(i);
          } else {
            aResult.SetTo(lengthListInfo.mLengthLists[i].GetBaseValue(),
                          &aValue);
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
      for (i = 0; i < numberListInfo.mNumberListCount; i++) {
        if (aAttribute == *numberListInfo.mNumberListInfo[i].mName) {
          rv = numberListInfo.mNumberLists[i].SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            numberListInfo.Reset(i);
          } else {
            aResult.SetTo(numberListInfo.mNumberLists[i].GetBaseValue(),
                          &aValue);
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
        SVGAnimatedPointList* pointList = GetAnimatedPointList();
        if (pointList) {
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
        SVGAnimatedPathSegList* segList = GetAnimPathSegList();
        if (segList) {
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
      // Check for nsSVGNumber2 attribute
      NumberAttributesInfo numberInfo = GetNumberInfo();
      for (i = 0; i < numberInfo.mNumberCount; i++) {
        if (aAttribute == *numberInfo.mNumberInfo[i].mName) {
          rv = numberInfo.mNumbers[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            numberInfo.Reset(i);
          } else {
            aResult.SetTo(numberInfo.mNumbers[i].GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGNumberPair attribute
      NumberPairAttributesInfo numberPairInfo = GetNumberPairInfo();
      for (i = 0; i < numberPairInfo.mNumberPairCount; i++) {
        if (aAttribute == *numberPairInfo.mNumberPairInfo[i].mName) {
          rv = numberPairInfo.mNumberPairs[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            numberPairInfo.Reset(i);
          } else {
            aResult.SetTo(numberPairInfo.mNumberPairs[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGInteger attribute
      IntegerAttributesInfo integerInfo = GetIntegerInfo();
      for (i = 0; i < integerInfo.mIntegerCount; i++) {
        if (aAttribute == *integerInfo.mIntegerInfo[i].mName) {
          rv = integerInfo.mIntegers[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            integerInfo.Reset(i);
          } else {
            aResult.SetTo(integerInfo.mIntegers[i].GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGIntegerPair attribute
      IntegerPairAttributesInfo integerPairInfo = GetIntegerPairInfo();
      for (i = 0; i < integerPairInfo.mIntegerPairCount; i++) {
        if (aAttribute == *integerPairInfo.mIntegerPairInfo[i].mName) {
          rv =
            integerPairInfo.mIntegerPairs[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            integerPairInfo.Reset(i);
          } else {
            aResult.SetTo(integerPairInfo.mIntegerPairs[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGAngle attribute
      AngleAttributesInfo angleInfo = GetAngleInfo();
      for (i = 0; i < angleInfo.mAngleCount; i++) {
        if (aAttribute == *angleInfo.mAngleInfo[i].mName) {
          rv = angleInfo.mAngles[i].SetBaseValueString(aValue, this, false);
          if (NS_FAILED(rv)) {
            angleInfo.Reset(i);
          } else {
            aResult.SetTo(angleInfo.mAngles[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGBoolean attribute
      BooleanAttributesInfo booleanInfo = GetBooleanInfo();
      for (i = 0; i < booleanInfo.mBooleanCount; i++) {
        if (aAttribute == *booleanInfo.mBooleanInfo[i].mName) {
          nsIAtom *valAtom = NS_GetStaticAtom(aValue);
          rv = valAtom ? booleanInfo.mBooleans[i].SetBaseValueAtom(valAtom, this) :
                 NS_ERROR_DOM_SYNTAX_ERR;
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
      // Check for nsSVGEnum attribute
      EnumAttributesInfo enumInfo = GetEnumInfo();
      for (i = 0; i < enumInfo.mEnumCount; i++) {
        if (aAttribute == *enumInfo.mEnumInfo[i].mName) {
          nsCOMPtr<nsIAtom> valAtom = NS_Atomize(aValue);
          rv = enumInfo.mEnums[i].SetBaseValueAtom(valAtom, this);
          if (NS_FAILED(rv)) {
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
      for (i = 0; i < stringListInfo.mStringListCount; i++) {
        if (aAttribute == *stringListInfo.mStringListInfo[i].mName) {
          rv = stringListInfo.mStringLists[i].SetValue(aValue);
          if (NS_FAILED(rv)) {
            stringListInfo.Reset(i);
          } else {
            aResult.SetTo(stringListInfo.mStringLists[i], &aValue);
            didSetResult = true;
          }
          foundMatch = true;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGViewBox attribute
      if (aAttribute == nsGkAtoms::viewBox) {
        nsSVGViewBox* viewBox = GetViewBox();
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
      // Check for SVGAnimatedPreserveAspectRatio attribute
      } else if (aAttribute == nsGkAtoms::preserveAspectRatio) {
        SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
          GetPreserveAspectRatio();
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
        // nsSVGAnimatedTransformList is/has been allocated:
        nsSVGAnimatedTransformList *transformList =
          GetAnimatedTransformList(DO_ALLOCATE);
        rv = transformList->SetBaseValueString(aValue);
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
  }

  if (!foundMatch) {
    // Check for nsSVGString attribute
    StringAttributesInfo stringInfo = GetStringInfo();
    for (uint32_t i = 0; i < stringInfo.mStringCount; i++) {
      if (aNamespaceID == stringInfo.mStringInfo[i].mNamespaceID &&
          aAttribute == *stringInfo.mStringInfo[i].mName) {
        stringInfo.mStrings[i].SetBaseValue(aValue, this, false);
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

  return nsSVGElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                          aResult);
}

void
nsSVGElement::UnsetAttrInternal(int32_t aNamespaceID, nsIAtom* aName,
                                bool aNotify)
{
  // XXXbz there's a bunch of redundancy here with AfterSetAttr.
  // Maybe consolidate?

  if (aNamespaceID == kNameSpaceID_None) {
    // If this is an svg presentation attribute, remove declaration block to
    // force an update
    if (IsAttributeMapped(aName)) {
      mContentDeclarationBlock = nullptr;
    }

    if (IsEventAttributeName(aName)) {
      EventListenerManager* manager = GetExistingListenerManager();
      if (manager) {
        nsIAtom* eventName = GetEventNameForAttr(aName);
        manager->RemoveEventHandler(eventName, EmptyString());
      }
      return;
    }
    
    // Check if this is a length attribute going away
    LengthAttributesInfo lenInfo = GetLengthInfo();

    for (uint32_t i = 0; i < lenInfo.mLengthCount; i++) {
      if (aName == *lenInfo.mLengthInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        lenInfo.Reset(i);
        return;
      }
    }

    // Check if this is a length list attribute going away
    LengthListAttributesInfo lengthListInfo = GetLengthListInfo();

    for (uint32_t i = 0; i < lengthListInfo.mLengthListCount; i++) {
      if (aName == *lengthListInfo.mLengthListInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        lengthListInfo.Reset(i);
        return;
      }
    }

    // Check if this is a number list attribute going away
    NumberListAttributesInfo numberListInfo = GetNumberListInfo();

    for (uint32_t i = 0; i < numberListInfo.mNumberListCount; i++) {
      if (aName == *numberListInfo.mNumberListInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        numberListInfo.Reset(i);
        return;
      }
    }

    // Check if this is a point list attribute going away
    if (GetPointListAttrName() == aName) {
      SVGAnimatedPointList *pointList = GetAnimatedPointList();
      if (pointList) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        pointList->ClearBaseValue();
        return;
      }
    }

    // Check if this is a path segment list attribute going away
    if (GetPathDataAttrName() == aName) {
      SVGAnimatedPathSegList *segList = GetAnimPathSegList();
      if (segList) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        segList->ClearBaseValue();
        return;
      }
    }

    // Check if this is a number attribute going away
    NumberAttributesInfo numInfo = GetNumberInfo();

    for (uint32_t i = 0; i < numInfo.mNumberCount; i++) {
      if (aName == *numInfo.mNumberInfo[i].mName) {
        numInfo.Reset(i);
        return;
      }
    }

    // Check if this is a number pair attribute going away
    NumberPairAttributesInfo numPairInfo = GetNumberPairInfo();

    for (uint32_t i = 0; i < numPairInfo.mNumberPairCount; i++) {
      if (aName == *numPairInfo.mNumberPairInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        numPairInfo.Reset(i);
        return;
      }
    }

    // Check if this is an integer attribute going away
    IntegerAttributesInfo intInfo = GetIntegerInfo();

    for (uint32_t i = 0; i < intInfo.mIntegerCount; i++) {
      if (aName == *intInfo.mIntegerInfo[i].mName) {
        intInfo.Reset(i);
        return;
      }
    }

    // Check if this is an integer pair attribute going away
    IntegerPairAttributesInfo intPairInfo = GetIntegerPairInfo();

    for (uint32_t i = 0; i < intPairInfo.mIntegerPairCount; i++) {
      if (aName == *intPairInfo.mIntegerPairInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        intPairInfo.Reset(i);
        return;
      }
    }

    // Check if this is an angle attribute going away
    AngleAttributesInfo angleInfo = GetAngleInfo();

    for (uint32_t i = 0; i < angleInfo.mAngleCount; i++) {
      if (aName == *angleInfo.mAngleInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        angleInfo.Reset(i);
        return;
      }
    }

    // Check if this is a boolean attribute going away
    BooleanAttributesInfo boolInfo = GetBooleanInfo();

    for (uint32_t i = 0; i < boolInfo.mBooleanCount; i++) {
      if (aName == *boolInfo.mBooleanInfo[i].mName) {
        boolInfo.Reset(i);
        return;
      }
    }

    // Check if this is an enum attribute going away
    EnumAttributesInfo enumInfo = GetEnumInfo();

    for (uint32_t i = 0; i < enumInfo.mEnumCount; i++) {
      if (aName == *enumInfo.mEnumInfo[i].mName) {
        enumInfo.Reset(i);
        return;
      }
    }

    // Check if this is a nsViewBox attribute going away
    if (aName == nsGkAtoms::viewBox) {
      nsSVGViewBox* viewBox = GetViewBox();
      if (viewBox) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        viewBox->Init();
        return;
      }
    }

    // Check if this is a preserveAspectRatio attribute going away
    if (aName == nsGkAtoms::preserveAspectRatio) {
      SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
        GetPreserveAspectRatio();
      if (preserveAspectRatio) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        preserveAspectRatio->Init();
        return;
      }
    }

    // Check if this is a transform list attribute going away
    if (GetTransformListAttrName() == aName) {
      nsSVGAnimatedTransformList *transformList = GetAnimatedTransformList();
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

    for (uint32_t i = 0; i < stringListInfo.mStringListCount; i++) {
      if (aName == *stringListInfo.mStringListInfo[i].mName) {
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

  for (uint32_t i = 0; i < stringInfo.mStringCount; i++) {
    if (aNamespaceID == stringInfo.mStringInfo[i].mNamespaceID &&
        aName == *stringInfo.mStringInfo[i].mName) {
      stringInfo.Reset(i);
      return;
    }
  }
}

nsresult
nsSVGElement::UnsetAttr(int32_t aNamespaceID, nsIAtom* aName,
                        bool aNotify)
{
  UnsetAttrInternal(aNamespaceID, aName, aNotify);
  return nsSVGElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

nsChangeHint
nsSVGElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                     int32_t aModType) const
{
  nsChangeHint retval =
    nsSVGElementBase::GetAttributeChangeHint(aAttribute, aModType);

  nsCOMPtr<SVGTests> tests = do_QueryObject(const_cast<nsSVGElement*>(this));
  if (tests && tests->IsConditionalProcessingAttribute(aAttribute)) {
    // It would be nice to only reconstruct the frame if the value returned by
    // SVGTests::PassesConditionalProcessingTests has changed, but we don't
    // know that
    retval |= nsChangeHint_ReconstructFrame;
  }
  return retval;
}

bool
nsSVGElement::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~eCONTENT);
}

void
nsSVGElement::NodeInfoChanged(nsIDocument* aOldDoc)
{
  nsSVGElementBase::NodeInfoChanged(aOldDoc);
  aOldDoc->UnscheduleSVGForPresAttrEvaluation(this);
  mContentDeclarationBlock = nullptr;
  OwnerDoc()->ScheduleSVGForPresAttrEvaluation(this);
}

NS_IMETHODIMP
nsSVGElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
#ifdef DEBUG
//  printf("nsSVGElement(%p)::WalkContentStyleRules()\n", this);
#endif
  if (!mContentDeclarationBlock) {
    UpdateContentDeclarationBlock(StyleBackendType::Gecko);
  }

  if (mContentDeclarationBlock) {
    css::Declaration* declaration = mContentDeclarationBlock->AsGecko();
    declaration->SetImmutable();
    aRuleWalker->Forward(declaration);
  }

  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsSVGElement::IsAttributeMapped(const nsIAtom* name) const
{
  if (name == nsGkAtoms::lang) {
    return true;
  }
  return nsSVGElementBase::IsAttributeMapped(name);
}

// PresentationAttributes-FillStroke
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sFillStrokeMap[] = {
  { &nsGkAtoms::fill },
  { &nsGkAtoms::fill_opacity },
  { &nsGkAtoms::fill_rule },
  { &nsGkAtoms::paint_order },
  { &nsGkAtoms::stroke },
  { &nsGkAtoms::stroke_dasharray },
  { &nsGkAtoms::stroke_dashoffset },
  { &nsGkAtoms::stroke_linecap },
  { &nsGkAtoms::stroke_linejoin },
  { &nsGkAtoms::stroke_miterlimit },
  { &nsGkAtoms::stroke_opacity },
  { &nsGkAtoms::stroke_width },
  { &nsGkAtoms::vector_effect },
  { nullptr }
};

// PresentationAttributes-Graphics
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sGraphicsMap[] = {
  { &nsGkAtoms::clip_path },
  { &nsGkAtoms::clip_rule },
  { &nsGkAtoms::colorInterpolation },
  { &nsGkAtoms::cursor },
  { &nsGkAtoms::display },
  { &nsGkAtoms::filter },
  { &nsGkAtoms::image_rendering },
  { &nsGkAtoms::mask },
  { &nsGkAtoms::opacity },
  { &nsGkAtoms::pointer_events },
  { &nsGkAtoms::shape_rendering },
  { &nsGkAtoms::text_rendering },
  { &nsGkAtoms::visibility },
  { nullptr }
};

// PresentationAttributes-TextContentElements
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sTextContentElementsMap[] = {
  // Properties that we don't support are commented out.
  // { &nsGkAtoms::alignment_baseline },
  // { &nsGkAtoms::baseline_shift },
  { &nsGkAtoms::direction },
  { &nsGkAtoms::dominant_baseline },
  { &nsGkAtoms::letter_spacing },
  { &nsGkAtoms::text_anchor },
  { &nsGkAtoms::text_decoration },
  { &nsGkAtoms::unicode_bidi },
  { &nsGkAtoms::word_spacing },
  { &nsGkAtoms::writing_mode },
  { nullptr }
};

// PresentationAttributes-FontSpecification
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sFontSpecificationMap[] = {
  { &nsGkAtoms::font_family },
  { &nsGkAtoms::font_size },
  { &nsGkAtoms::font_size_adjust },
  { &nsGkAtoms::font_stretch },
  { &nsGkAtoms::font_style },
  { &nsGkAtoms::font_variant },
  { &nsGkAtoms::fontWeight },  
  { nullptr }
};

// PresentationAttributes-GradientStop
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sGradientStopMap[] = {
  { &nsGkAtoms::stop_color },
  { &nsGkAtoms::stop_opacity },
  { nullptr }
};

// PresentationAttributes-Viewports
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sViewportsMap[] = {
  { &nsGkAtoms::overflow },
  { &nsGkAtoms::clip },
  { nullptr }
};

// PresentationAttributes-Makers
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sMarkersMap[] = {
  { &nsGkAtoms::marker_end },
  { &nsGkAtoms::marker_mid },
  { &nsGkAtoms::marker_start },
  { nullptr }
};

// PresentationAttributes-Color
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sColorMap[] = {
  { &nsGkAtoms::color },
  { nullptr }
};

// PresentationAttributes-Filters
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sFiltersMap[] = {
  { &nsGkAtoms::colorInterpolationFilters },
  { nullptr }
};

// PresentationAttributes-feFlood
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sFEFloodMap[] = {
  { &nsGkAtoms::flood_color },
  { &nsGkAtoms::flood_opacity },
  { nullptr }
};

// PresentationAttributes-LightingEffects
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sLightingEffectsMap[] = {
  { &nsGkAtoms::lighting_color },
  { nullptr }
};

// PresentationAttributes-mask
/* static */ const Element::MappedAttributeEntry
nsSVGElement::sMaskMap[] = {
  { &nsGkAtoms::mask_type },
  { nullptr }
};

//----------------------------------------------------------------------
// nsIDOMElement methods

// forwarded to Element implementations


//----------------------------------------------------------------------
// nsIDOMSVGElement methods

NS_IMETHODIMP
nsSVGElement::GetOwnerSVGElement(nsIDOMSVGElement * *aOwnerSVGElement)
{
  NS_IF_ADDREF(*aOwnerSVGElement = GetOwnerSVGElement());
  return NS_OK;
}

SVGSVGElement*
nsSVGElement::GetOwnerSVGElement()
{
  return GetCtx(); // this may return nullptr
}

NS_IMETHODIMP
nsSVGElement::GetViewportElement(nsIDOMSVGElement * *aViewportElement)
{
  nsSVGElement* elem = GetViewportElement();
  NS_ADDREF(*aViewportElement = elem);
  return NS_OK;
}

nsSVGElement*
nsSVGElement::GetViewportElement()
{
  return SVGContentUtils::GetNearestViewportElement(this);
}

already_AddRefed<SVGAnimatedString>
nsSVGElement::ClassName()
{
  return mClassAttribute.ToDOMAnimatedString(this);
}

bool
nsSVGElement::IsSVGFocusable(bool* aIsFocusable, int32_t* aTabIndex)
{
  nsIDocument* doc = GetComposedDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    // In designMode documents we only allow focusing the document.
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;

    return true;
  }

  int32_t tabIndex = TabIndex();

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  // If a tabindex is specified at all, or the default tabindex is 0, we're focusable
  *aIsFocusable =
    tabIndex >= 0 || HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex);

  return false;
}

bool
nsSVGElement::IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse)
{
  bool isFocusable = false;
  IsSVGFocusable(&isFocusable, aTabIndex);
  return isFocusable;
}

//------------------------------------------------------------------------
// Helper class: MappedAttrParser, for parsing values of mapped attributes

namespace {

class MOZ_STACK_CLASS MappedAttrParser {
public:
  MappedAttrParser(css::Loader* aLoader,
                   nsIURI* aDocURI,
                   already_AddRefed<nsIURI> aBaseURI,
                   nsSVGElement* aElement,
                   StyleBackendType aBackend);
  ~MappedAttrParser();

  // Parses a mapped attribute value.
  void ParseMappedAttrValue(nsIAtom* aMappedAttrName,
                            const nsAString& aMappedAttrValue);

  // If we've parsed any values for mapped attributes, this method returns the
  // already_AddRefed css::Declaration that incorporates the parsed
  // values. Otherwise, this method returns null.
  already_AddRefed<DeclarationBlock> GetDeclarationBlock();

private:
  // MEMBER DATA
  // -----------
  nsCSSParser       mParser;

  // Arguments for nsCSSParser::ParseProperty
  nsIURI*           mDocURI;
  nsCOMPtr<nsIURI>  mBaseURI;

  // Declaration for storing parsed values (lazily initialized)
  RefPtr<DeclarationBlock> mDecl;

  // For reporting use counters
  nsSVGElement*     mElement;

  StyleBackendType mBackend;
};

MappedAttrParser::MappedAttrParser(css::Loader* aLoader,
                                   nsIURI* aDocURI,
                                   already_AddRefed<nsIURI> aBaseURI,
                                   nsSVGElement* aElement,
                                   StyleBackendType aBackend)
  : mParser(aLoader), mDocURI(aDocURI), mBaseURI(aBaseURI),
    mElement(aElement), mBackend(aBackend)
{
}

MappedAttrParser::~MappedAttrParser()
{
  MOZ_ASSERT(!mDecl,
             "If mDecl was initialized, it should have been returned via "
             "GetDeclarationBlock (and had its pointer cleared)");
}

void
MappedAttrParser::ParseMappedAttrValue(nsIAtom* aMappedAttrName,
                                       const nsAString& aMappedAttrValue)
{
  if (!mDecl) {
    if (mBackend == StyleBackendType::Gecko) {
      mDecl = new css::Declaration();
      mDecl->AsGecko()->InitializeEmpty();
    } else {
      mDecl = new ServoDeclarationBlock();
    }
  }

  // Get the nsCSSPropertyID ID for our mapped attribute.
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(nsDependentAtomString(aMappedAttrName),
                               CSSEnabledState::eForAllContent);
  if (propertyID != eCSSProperty_UNKNOWN) {
    bool changed = false; // outparam for ParseProperty.
    if (mBackend == StyleBackendType::Gecko) {
      mParser.ParseProperty(propertyID, aMappedAttrValue, mDocURI, mBaseURI,
                            mElement->NodePrincipal(), mDecl->AsGecko(), &changed, false, true);
    } else {
      NS_ConvertUTF16toUTF8 value(aMappedAttrValue);
      // FIXME (bug 1343964): Figure out a better solution for sending the base uri to servo
      RefPtr<URLExtraData> data = new URLExtraData(mBaseURI, mDocURI,
                                                   mElement->NodePrincipal());
      changed = Servo_DeclarationBlock_SetPropertyById(
        mDecl->AsServo()->Raw(), propertyID, &value, false, data,
        ParsingMode::AllowUnitlessLength, mElement->OwnerDoc()->GetCompatibilityMode());
    }

    if (changed) {
      // The normal reporting of use counters by the nsCSSParser won't happen
      // since it doesn't have a sheet.
      if (nsCSSProps::IsShorthand(propertyID)) {
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, propertyID,
                                             CSSEnabledState::eForAllContent) {
          UseCounter useCounter = nsCSSProps::UseCounterFor(*subprop);
          if (useCounter != eUseCounter_UNKNOWN) {
            mElement->OwnerDoc()->SetDocumentAndPageUseCounter(useCounter);
          }
        }
      } else {
        UseCounter useCounter = nsCSSProps::UseCounterFor(propertyID);
        if (useCounter != eUseCounter_UNKNOWN) {
          mElement->OwnerDoc()->SetDocumentAndPageUseCounter(useCounter);
        }
      }
    }
    return;
  }
  MOZ_ASSERT(aMappedAttrName == nsGkAtoms::lang,
             "Only 'lang' should be unrecognized!");
  // nsCSSParser doesn't know about 'lang', so we need to handle it specially.
  if (aMappedAttrName == nsGkAtoms::lang) {
    propertyID = eCSSProperty__x_lang;
    if (mBackend == StyleBackendType::Gecko) {
      nsCSSExpandedDataBlock block;
      mDecl->AsGecko()->ExpandTo(&block);
      nsCSSValue cssValue(PromiseFlatString(aMappedAttrValue), eCSSUnit_Ident);
      block.AddLonghandProperty(propertyID, cssValue);
      mDecl->AsGecko()->ValueAppended(propertyID);
      mDecl->AsGecko()->CompressFrom(&block);
    } else {
      nsCOMPtr<nsIAtom> atom = NS_Atomize(aMappedAttrValue);
      Servo_DeclarationBlock_SetIdentStringValue(mDecl->AsServo()->Raw(), propertyID, atom);
    }
  }
}

already_AddRefed<DeclarationBlock>
MappedAttrParser::GetDeclarationBlock()
{
  return mDecl.forget();
}

} // namespace

//----------------------------------------------------------------------
// Implementation Helpers:

void
nsSVGElement::UpdateContentDeclarationBlock(mozilla::StyleBackendType aBackend)
{
  NS_ASSERTION(!mContentDeclarationBlock,
               "we already have a content declaration block");

  uint32_t attrCount = mAttrsAndChildren.AttrCount();
  if (!attrCount) {
    // nothing to do
    return;
  }

  nsIDocument* doc = OwnerDoc();
  MappedAttrParser mappedAttrParser(doc->CSSLoader(), doc->GetDocumentURI(),
                                    GetBaseURI(), this, aBackend);

  for (uint32_t i = 0; i < attrCount; ++i) {
    const nsAttrName* attrName = mAttrsAndChildren.AttrNameAt(i);
    if (!attrName->IsAtom() || !IsAttributeMapped(attrName->Atom()))
      continue;

    if (attrName->NamespaceID() != kNameSpaceID_None &&
        !attrName->Equals(nsGkAtoms::lang, kNameSpaceID_XML)) {
      continue;
    }

    if (attrName->Equals(nsGkAtoms::lang, kNameSpaceID_None) &&
        HasAttr(kNameSpaceID_XML, nsGkAtoms::lang)) {
      continue; // xml:lang has precedence
    }

    if (IsSVGElement(nsGkAtoms::svg)) {
      // Special case: we don't want <svg> 'width'/'height' mapped into style
      // if the attribute value isn't a valid <length> according to SVG (which
      // only supports a subset of the CSS <length> values). We don't enforce
      // this by checking the attribute value in SVGSVGElement::
      // IsAttributeMapped since we don't want that method to depend on the
      // value of the attribute that is being checked. Rather we just prevent
      // the actual mapping here, as necessary.
      if (attrName->Atom() == nsGkAtoms::width &&
          !GetAnimatedLength(nsGkAtoms::width)->HasBaseVal()) {
        continue;
      }
      if (attrName->Atom() == nsGkAtoms::height &&
          !GetAnimatedLength(nsGkAtoms::height)->HasBaseVal()) {
        continue;
      }
    }

    nsAutoString value;
    mAttrsAndChildren.AttrAt(i)->ToString(value);
    mappedAttrParser.ParseMappedAttrValue(attrName->Atom(), value);
  }
  mContentDeclarationBlock = mappedAttrParser.GetDeclarationBlock();
}

const DeclarationBlock*
nsSVGElement::GetContentDeclarationBlock() const
{
  return mContentDeclarationBlock;
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
nsAttrValue
nsSVGElement::WillChangeValue(nsIAtom* aName)
{
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
  if (attrValue &&
      nsContentUtils::HasMutationListeners(this,
                                           NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                           this)) {
    emptyOrOldAttrValue.SetToSerialized(*attrValue);
  }

  uint8_t modType = attrValue
                  ? static_cast<uint8_t>(nsIDOMMutationEvent::MODIFICATION)
                  : static_cast<uint8_t>(nsIDOMMutationEvent::ADDITION);
  nsNodeUtils::AttributeWillChange(this, kNameSpaceID_None, aName, modType,
                                   nullptr);

  // This is not strictly correct--the attribute value parameter for
  // BeforeSetAttr should reflect the value that *will* be set but that implies
  // allocating, e.g. an extra nsSVGLength2, and isn't necessary at the moment
  // since no SVG elements overload BeforeSetAttr. For now we just pass the
  // current value.
  nsAttrValueOrString attrStringOrValue(attrValue ? *attrValue
                                                  : emptyOrOldAttrValue);
  DebugOnly<nsresult> rv =
    BeforeSetAttr(kNameSpaceID_None, aName, &attrStringOrValue,
                  kNotifyDocumentObservers);
  // SVG elements aren't expected to overload BeforeSetAttr in such a way that
  // it may fail. So long as this is the case we don't need to check and pass on
  // the return value which simplifies the calling code significantly.
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Unexpected failure from BeforeSetAttr");

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
void
nsSVGElement::DidChangeValue(nsIAtom* aName,
                             const nsAttrValue& aEmptyOrOldValue,
                             nsAttrValue& aNewValue)
{
  bool hasListeners =
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);
  uint8_t modType = HasAttr(kNameSpaceID_None, aName)
                  ? static_cast<uint8_t>(nsIDOMMutationEvent::MODIFICATION)
                  : static_cast<uint8_t>(nsIDOMMutationEvent::ADDITION);

  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL,
                               kNotifyDocumentObservers);
  // XXX Really, the fourth argument to SetAttrAndNotify should be null if
  // aEmptyOrOldValue does not represent the actual previous value of the
  // attribute, but currently SVG elements do not even use the old attribute
  // value in |AfterSetAttr|, so this should be ok.
  SetAttrAndNotify(kNameSpaceID_None, aName, nullptr, &aEmptyOrOldValue,
                   aNewValue, modType, hasListeners, kNotifyDocumentObservers,
                   kCallAfterSetAttr, document, updateBatch);
}

void
nsSVGElement::MaybeSerializeAttrBeforeRemoval(nsIAtom* aName, bool aNotify)
{
  if (!aNotify ||
      !nsContentUtils::HasMutationListeners(this,
                                            NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                            this)) {
    return;
  }

  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(aName);
  if (!attrValue)
    return;

  nsAutoString serializedValue;
  attrValue->ToString(serializedValue);
  nsAttrValue oldAttrValue(serializedValue);
  bool oldValueSet;
  mAttrsAndChildren.SetAndSwapAttr(aName, oldAttrValue, &oldValueSet);
}

/* static */
nsIAtom* nsSVGElement::GetEventNameForAttr(nsIAtom* aAttr)
{
  if (aAttr == nsGkAtoms::onload)
    return nsGkAtoms::onSVGLoad;
  if (aAttr == nsGkAtoms::onunload)
    return nsGkAtoms::onSVGUnload;
  if (aAttr == nsGkAtoms::onresize)
    return nsGkAtoms::onSVGResize;
  if (aAttr == nsGkAtoms::onscroll)
    return nsGkAtoms::onSVGScroll;
  if (aAttr == nsGkAtoms::onzoom)
    return nsGkAtoms::onSVGZoom;
  if (aAttr == nsGkAtoms::onbegin)
    return nsGkAtoms::onbeginEvent;
  if (aAttr == nsGkAtoms::onrepeat)
    return nsGkAtoms::onrepeatEvent;
  if (aAttr == nsGkAtoms::onend)
    return nsGkAtoms::onendEvent;

  return aAttr;
}

SVGSVGElement *
nsSVGElement::GetCtx() const
{
  nsIContent* ancestor = GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVGElement()) {
    if (ancestor->IsSVGElement(nsGkAtoms::foreignObject)) {
      return nullptr;
    }
    if (ancestor->IsSVGElement(nsGkAtoms::svg)) {
      return static_cast<SVGSVGElement*>(ancestor);
    }
    ancestor = ancestor->GetFlattenedTreeParent();
  }

  // we don't have an ancestor <svg> element...
  return nullptr;
}

/* virtual */ gfxMatrix
nsSVGElement::PrependLocalTransformsTo(
  const gfxMatrix &aMatrix, SVGTransformTypes aWhich) const
{
  return aMatrix;
}

nsSVGElement::LengthAttributesInfo
nsSVGElement::GetLengthInfo()
{
  return LengthAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::LengthAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mLengths[aAttrEnum].Init(mLengthInfo[aAttrEnum].mCtxType,
                           aAttrEnum,
                           mLengthInfo[aAttrEnum].mDefaultValue,
                           mLengthInfo[aAttrEnum].mDefaultUnitType);
}

void
nsSVGElement::SetLength(nsIAtom* aName, const nsSVGLength2 &aLength)
{
  LengthAttributesInfo lengthInfo = GetLengthInfo();

  for (uint32_t i = 0; i < lengthInfo.mLengthCount; i++) {
    if (aName == *lengthInfo.mLengthInfo[i].mName) {
      lengthInfo.mLengths[i] = aLength;
      DidAnimateLength(i);
      return;
    }
  }
  MOZ_ASSERT(false, "no length found to set");
}

nsAttrValue
nsSVGElement::WillChangeLength(uint8_t aAttrEnum)
{
  return WillChangeValue(*GetLengthInfo().mLengthInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeLength(uint8_t aAttrEnum,
                              const nsAttrValue& aEmptyOrOldValue)
{
  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mLengthCount > 0,
               "DidChangeLength on element with no length attribs");
  NS_ASSERTION(aAttrEnum < info.mLengthCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mLengths[aAttrEnum], nullptr);

  DidChangeValue(*info.mLengthInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateLength(uint8_t aAttrEnum)
{
  ClearAnyCachedPath();

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    LengthAttributesInfo info = GetLengthInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mLengthInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGLength2*
nsSVGElement::GetAnimatedLength(const nsIAtom *aAttrName)
{
  LengthAttributesInfo lengthInfo = GetLengthInfo();

  for (uint32_t i = 0; i < lengthInfo.mLengthCount; i++) {
    if (aAttrName == *lengthInfo.mLengthInfo[i].mName) {
      return &lengthInfo.mLengths[i];
    }
  }
  MOZ_ASSERT(false, "no matching length found");
  return nullptr;
}

void
nsSVGElement::GetAnimatedLengthValues(float *aFirst, ...)
{
  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mLengthCount > 0,
               "GetAnimatedLengthValues on element with no length attribs");

  SVGSVGElement *ctx = nullptr;

  float *f = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (f && i < info.mLengthCount) {
    uint8_t type = info.mLengths[i].GetSpecifiedUnitType();
    if (!ctx) {
      if (type != nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER &&
          type != nsIDOMSVGLength::SVG_LENGTHTYPE_PX)
        ctx = GetCtx();
    }
    if (type == nsIDOMSVGLength::SVG_LENGTHTYPE_EMS ||
        type == nsIDOMSVGLength::SVG_LENGTHTYPE_EXS)
      *f = info.mLengths[i++].GetAnimValue(this);
    else
      *f = info.mLengths[i++].GetAnimValue(ctx);
    f = va_arg(args, float*);
  }

  va_end(args);
}

nsSVGElement::LengthListAttributesInfo
nsSVGElement::GetLengthListInfo()
{
  return LengthListAttributesInfo(nullptr, nullptr, 0);
}

void
nsSVGElement::LengthListAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mLengthLists[aAttrEnum].ClearBaseValue(aAttrEnum);
  // caller notifies
}

nsAttrValue
nsSVGElement::WillChangeLengthList(uint8_t aAttrEnum)
{
  return WillChangeValue(*GetLengthListInfo().mLengthListInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeLengthList(uint8_t aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(info.mLengthListCount > 0,
               "DidChangeLengthList on element with no length list attribs");
  NS_ASSERTION(aAttrEnum < info.mLengthListCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mLengthLists[aAttrEnum].GetBaseValue(), nullptr);

  DidChangeValue(*info.mLengthListInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateLengthList(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    LengthListAttributesInfo info = GetLengthListInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mLengthListInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

void
nsSVGElement::GetAnimatedLengthListValues(SVGUserUnitList *aFirst, ...)
{
  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(info.mLengthListCount > 0,
               "GetAnimatedLengthListValues on element with no length list attribs");

  SVGUserUnitList *list = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (list && i < info.mLengthListCount) {
    list->Init(&(info.mLengthLists[i].GetAnimValue()), this, info.mLengthListInfo[i].mAxis);
    ++i;
    list = va_arg(args, SVGUserUnitList*);
  }

  va_end(args);
}

SVGAnimatedLengthList*
nsSVGElement::GetAnimatedLengthList(uint8_t aAttrEnum)
{
  LengthListAttributesInfo info = GetLengthListInfo();
  if (aAttrEnum < info.mLengthListCount) {
    return &(info.mLengthLists[aAttrEnum]);
  }
  NS_NOTREACHED("Bad attrEnum");
  return nullptr;
}


nsSVGElement::NumberListAttributesInfo
nsSVGElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(nullptr, nullptr, 0);
}

void
nsSVGElement::NumberListAttributesInfo::Reset(uint8_t aAttrEnum)
{
  MOZ_ASSERT(aAttrEnum < mNumberListCount, "Bad attr enum");
  mNumberLists[aAttrEnum].ClearBaseValue(aAttrEnum);
  // caller notifies
}

nsAttrValue
nsSVGElement::WillChangeNumberList(uint8_t aAttrEnum)
{
  return WillChangeValue(*GetNumberListInfo().mNumberListInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeNumberList(uint8_t aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  NumberListAttributesInfo info = GetNumberListInfo();

  MOZ_ASSERT(info.mNumberListCount > 0,
             "DidChangeNumberList on element with no number list attribs");
  MOZ_ASSERT(aAttrEnum < info.mNumberListCount,
             "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mNumberLists[aAttrEnum].GetBaseValue(), nullptr);

  DidChangeValue(*info.mNumberListInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateNumberList(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    NumberListAttributesInfo info = GetNumberListInfo();
    MOZ_ASSERT(aAttrEnum < info.mNumberListCount, "aAttrEnum out of range");

    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mNumberListInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

SVGAnimatedNumberList*
nsSVGElement::GetAnimatedNumberList(uint8_t aAttrEnum)
{
  NumberListAttributesInfo info = GetNumberListInfo();
  if (aAttrEnum < info.mNumberListCount) {
    return &(info.mNumberLists[aAttrEnum]);
  }
  MOZ_ASSERT(false, "Bad attrEnum");
  return nullptr;
}

SVGAnimatedNumberList*
nsSVGElement::GetAnimatedNumberList(nsIAtom *aAttrName)
{
  NumberListAttributesInfo info = GetNumberListInfo();
  for (uint32_t i = 0; i < info.mNumberListCount; i++) {
    if (aAttrName == *info.mNumberListInfo[i].mName) {
      return &info.mNumberLists[i];
    }
  }
  MOZ_ASSERT(false, "Bad caller");
  return nullptr;
}

nsAttrValue
nsSVGElement::WillChangePointList()
{
  MOZ_ASSERT(GetPointListAttrName(),
             "Changing non-existent point list?");
  return WillChangeValue(GetPointListAttrName());
}

void
nsSVGElement::DidChangePointList(const nsAttrValue& aEmptyOrOldValue)
{
  MOZ_ASSERT(GetPointListAttrName(),
             "Changing non-existent point list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimatedPointList()->GetBaseValue(), nullptr);

  DidChangeValue(GetPointListAttrName(), aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimatePointList()
{
  MOZ_ASSERT(GetPointListAttrName(),
             "Animating non-existent path data?");

  ClearAnyCachedPath();

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            GetPointListAttrName(),
                            nsIDOMMutationEvent::SMIL);
  }
}

nsAttrValue
nsSVGElement::WillChangePathSegList()
{
  MOZ_ASSERT(GetPathDataAttrName(),
             "Changing non-existent path seg list?");
  return WillChangeValue(GetPathDataAttrName());
}

void
nsSVGElement::DidChangePathSegList(const nsAttrValue& aEmptyOrOldValue)
{
  MOZ_ASSERT(GetPathDataAttrName(),
             "Changing non-existent path seg list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimPathSegList()->GetBaseValue(), nullptr);

  DidChangeValue(GetPathDataAttrName(), aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimatePathSegList()
{
  MOZ_ASSERT(GetPathDataAttrName(),
             "Animating non-existent path data?");

  ClearAnyCachedPath();

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            GetPathDataAttrName(),
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGElement::NumberAttributesInfo
nsSVGElement::GetNumberInfo()
{
  return NumberAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::NumberAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mNumbers[aAttrEnum].Init(aAttrEnum,
                           mNumberInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeNumber(uint8_t aAttrEnum)
{
  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mNumberCount > 0,
               "DidChangeNumber on element with no number attribs");
  NS_ASSERTION(aAttrEnum < info.mNumberCount, "aAttrEnum out of range");

  nsAttrValue attrValue;
  attrValue.SetTo(info.mNumbers[aAttrEnum].GetBaseValue(), nullptr);

  SetParsedAttr(kNameSpaceID_None, *info.mNumberInfo[aAttrEnum].mName, nullptr,
                attrValue, true);
}

void
nsSVGElement::DidAnimateNumber(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    NumberAttributesInfo info = GetNumberInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mNumberInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

void
nsSVGElement::GetAnimatedNumberValues(float *aFirst, ...)
{
  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mNumberCount > 0,
               "GetAnimatedNumberValues on element with no number attribs");

  float *f = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (f && i < info.mNumberCount) {
    *f = info.mNumbers[i++].GetAnimValue();
    f = va_arg(args, float*);
  }
  va_end(args);
}

nsSVGElement::NumberPairAttributesInfo
nsSVGElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::NumberPairAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mNumberPairs[aAttrEnum].Init(aAttrEnum,
                               mNumberPairInfo[aAttrEnum].mDefaultValue1,
                               mNumberPairInfo[aAttrEnum].mDefaultValue2);
}

nsAttrValue
nsSVGElement::WillChangeNumberPair(uint8_t aAttrEnum)
{
  return WillChangeValue(*GetNumberPairInfo().mNumberPairInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeNumberPair(uint8_t aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  NumberPairAttributesInfo info = GetNumberPairInfo();

  NS_ASSERTION(info.mNumberPairCount > 0,
               "DidChangePairNumber on element with no number pair attribs");
  NS_ASSERTION(aAttrEnum < info.mNumberPairCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mNumberPairs[aAttrEnum], nullptr);

  DidChangeValue(*info.mNumberPairInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateNumberPair(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    NumberPairAttributesInfo info = GetNumberPairInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mNumberPairInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGElement::IntegerAttributesInfo
nsSVGElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::IntegerAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mIntegers[aAttrEnum].Init(aAttrEnum,
                            mIntegerInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeInteger(uint8_t aAttrEnum)
{
  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mIntegerCount > 0,
               "DidChangeInteger on element with no integer attribs");
  NS_ASSERTION(aAttrEnum < info.mIntegerCount, "aAttrEnum out of range");

  nsAttrValue attrValue;
  attrValue.SetTo(info.mIntegers[aAttrEnum].GetBaseValue(), nullptr);

  SetParsedAttr(kNameSpaceID_None, *info.mIntegerInfo[aAttrEnum].mName, nullptr,
                attrValue, true);
}

void
nsSVGElement::DidAnimateInteger(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    IntegerAttributesInfo info = GetIntegerInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mIntegerInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

void
nsSVGElement::GetAnimatedIntegerValues(int32_t *aFirst, ...)
{
  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mIntegerCount > 0,
               "GetAnimatedIntegerValues on element with no integer attribs");

  int32_t *n = aFirst;
  uint32_t i = 0;

  va_list args;
  va_start(args, aFirst);

  while (n && i < info.mIntegerCount) {
    *n = info.mIntegers[i++].GetAnimValue();
    n = va_arg(args, int32_t*);
  }
  va_end(args);
}

nsSVGElement::IntegerPairAttributesInfo
nsSVGElement::GetIntegerPairInfo()
{
  return IntegerPairAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::IntegerPairAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mIntegerPairs[aAttrEnum].Init(aAttrEnum,
                                mIntegerPairInfo[aAttrEnum].mDefaultValue1,
                                mIntegerPairInfo[aAttrEnum].mDefaultValue2);
}

nsAttrValue
nsSVGElement::WillChangeIntegerPair(uint8_t aAttrEnum)
{
  return WillChangeValue(
    *GetIntegerPairInfo().mIntegerPairInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeIntegerPair(uint8_t aAttrEnum,
                                   const nsAttrValue& aEmptyOrOldValue)
{
  IntegerPairAttributesInfo info = GetIntegerPairInfo();

  NS_ASSERTION(info.mIntegerPairCount > 0,
               "DidChangeIntegerPair on element with no integer pair attribs");
  NS_ASSERTION(aAttrEnum < info.mIntegerPairCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mIntegerPairs[aAttrEnum], nullptr);

  DidChangeValue(*info.mIntegerPairInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateIntegerPair(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    IntegerPairAttributesInfo info = GetIntegerPairInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mIntegerPairInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGElement::AngleAttributesInfo
nsSVGElement::GetAngleInfo()
{
  return AngleAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::AngleAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mAngles[aAttrEnum].Init(aAttrEnum, 
                          mAngleInfo[aAttrEnum].mDefaultValue,
                          mAngleInfo[aAttrEnum].mDefaultUnitType);
}

nsAttrValue
nsSVGElement::WillChangeAngle(uint8_t aAttrEnum)
{
  return WillChangeValue(*GetAngleInfo().mAngleInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeAngle(uint8_t aAttrEnum,
                             const nsAttrValue& aEmptyOrOldValue)
{
  AngleAttributesInfo info = GetAngleInfo();

  NS_ASSERTION(info.mAngleCount > 0,
               "DidChangeAngle on element with no angle attribs");
  NS_ASSERTION(aAttrEnum < info.mAngleCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mAngles[aAttrEnum], nullptr);

  DidChangeValue(*info.mAngleInfo[aAttrEnum].mName, aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimateAngle(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    AngleAttributesInfo info = GetAngleInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mAngleInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGElement::BooleanAttributesInfo
nsSVGElement::GetBooleanInfo()
{
  return BooleanAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::BooleanAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mBooleans[aAttrEnum].Init(aAttrEnum,
                            mBooleanInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeBoolean(uint8_t aAttrEnum)
{
  BooleanAttributesInfo info = GetBooleanInfo();

  NS_ASSERTION(info.mBooleanCount > 0,
               "DidChangeBoolean on element with no boolean attribs");
  NS_ASSERTION(aAttrEnum < info.mBooleanCount, "aAttrEnum out of range");

  nsAttrValue attrValue(info.mBooleans[aAttrEnum].GetBaseValueAtom());
  SetParsedAttr(kNameSpaceID_None, *info.mBooleanInfo[aAttrEnum].mName, nullptr,
                attrValue, true);
}

void
nsSVGElement::DidAnimateBoolean(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    BooleanAttributesInfo info = GetBooleanInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mBooleanInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGElement::EnumAttributesInfo
nsSVGElement::GetEnumInfo()
{
  return EnumAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::EnumAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mEnums[aAttrEnum].Init(aAttrEnum,
                         mEnumInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeEnum(uint8_t aAttrEnum)
{
  EnumAttributesInfo info = GetEnumInfo();

  NS_ASSERTION(info.mEnumCount > 0,
               "DidChangeEnum on element with no enum attribs");
  NS_ASSERTION(aAttrEnum < info.mEnumCount, "aAttrEnum out of range");

  nsAttrValue attrValue(info.mEnums[aAttrEnum].GetBaseValueAtom(this));
  SetParsedAttr(kNameSpaceID_None, *info.mEnumInfo[aAttrEnum].mName, nullptr,
                attrValue, true);
}

void
nsSVGElement::DidAnimateEnum(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    EnumAttributesInfo info = GetEnumInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mEnumInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGViewBox *
nsSVGElement::GetViewBox()
{
  return nullptr;
}

nsAttrValue
nsSVGElement::WillChangeViewBox()
{
  return WillChangeValue(nsGkAtoms::viewBox);
}

void
nsSVGElement::DidChangeViewBox(const nsAttrValue& aEmptyOrOldValue)
{
  nsSVGViewBox *viewBox = GetViewBox();

  NS_ASSERTION(viewBox, "DidChangeViewBox on element with no viewBox attrib");

  nsAttrValue newValue;
  newValue.SetTo(*viewBox, nullptr);

  DidChangeValue(nsGkAtoms::viewBox, aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimateViewBox()
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            nsGkAtoms::viewBox,
                            nsIDOMMutationEvent::SMIL);
  }
}

SVGAnimatedPreserveAspectRatio *
nsSVGElement::GetPreserveAspectRatio()
{
  return nullptr;
}

nsAttrValue
nsSVGElement::WillChangePreserveAspectRatio()
{
  return WillChangeValue(nsGkAtoms::preserveAspectRatio);
}

void
nsSVGElement::DidChangePreserveAspectRatio(const nsAttrValue& aEmptyOrOldValue)
{
  SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
    GetPreserveAspectRatio();

  NS_ASSERTION(preserveAspectRatio,
               "DidChangePreserveAspectRatio on element with no "
               "preserveAspectRatio attrib");

  nsAttrValue newValue;
  newValue.SetTo(*preserveAspectRatio, nullptr);

  DidChangeValue(nsGkAtoms::preserveAspectRatio, aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimatePreserveAspectRatio()
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            nsGkAtoms::preserveAspectRatio,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsAttrValue
nsSVGElement::WillChangeTransformList()
{
  return WillChangeValue(GetTransformListAttrName());
}

void
nsSVGElement::DidChangeTransformList(const nsAttrValue& aEmptyOrOldValue)
{
  MOZ_ASSERT(GetTransformListAttrName(),
             "Changing non-existent transform list?");

  // The transform attribute is being set, so we must ensure that the
  // SVGAnimatedTransformList is/has been allocated:
  nsAttrValue newValue;
  newValue.SetTo(GetAnimatedTransformList(DO_ALLOCATE)->GetBaseValue(), nullptr);

  DidChangeValue(GetTransformListAttrName(), aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimateTransformList(int32_t aModType)
{
  MOZ_ASSERT(GetTransformListAttrName(),
             "Animating non-existent transform data?");

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    nsIAtom *transformAttr = GetTransformListAttrName();
    frame->AttributeChanged(kNameSpaceID_None,
                            transformAttr,
                            aModType);
    // When script changes the 'transform' attribute, Element::SetAttrAndNotify
    // will call nsNodeUtills::AttributeChanged, under which
    // SVGTransformableElement::GetAttributeChangeHint will be called and an
    // appropriate change event posted to update our frame's overflow rects.
    // The SetAttrAndNotify doesn't happen for transform changes caused by
    // 'animateTransform' though (and sending out the mutation events that
    // nsNodeUtills::AttributeChanged dispatches would be inappropriate
    // anyway), so we need to post the change event ourself.
    nsChangeHint changeHint = GetAttributeChangeHint(transformAttr, aModType);
    if (changeHint) {
      nsLayoutUtils::PostRestyleEvent(this, nsRestyleHint(0), changeHint);
    }
  }
}

nsSVGElement::StringAttributesInfo
nsSVGElement::GetStringInfo()
{
  return StringAttributesInfo(nullptr, nullptr, 0);
}

void nsSVGElement::StringAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mStrings[aAttrEnum].Init(aAttrEnum);
}

void nsSVGElement::GetStringBaseValue(uint8_t aAttrEnum, nsAString& aResult) const
{
  nsSVGElement::StringAttributesInfo info = const_cast<nsSVGElement*>(this)->GetStringInfo();

  NS_ASSERTION(info.mStringCount > 0,
               "GetBaseValue on element with no string attribs");

  NS_ASSERTION(aAttrEnum < info.mStringCount, "aAttrEnum out of range");

  GetAttr(info.mStringInfo[aAttrEnum].mNamespaceID,
          *info.mStringInfo[aAttrEnum].mName, aResult);
}

void nsSVGElement::SetStringBaseValue(uint8_t aAttrEnum, const nsAString& aValue)
{
  nsSVGElement::StringAttributesInfo info = GetStringInfo();

  NS_ASSERTION(info.mStringCount > 0,
               "SetBaseValue on element with no string attribs");

  NS_ASSERTION(aAttrEnum < info.mStringCount, "aAttrEnum out of range");

  SetAttr(info.mStringInfo[aAttrEnum].mNamespaceID,
          *info.mStringInfo[aAttrEnum].mName, aValue, true);
}

void
nsSVGElement::DidAnimateString(uint8_t aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    StringAttributesInfo info = GetStringInfo();
    frame->AttributeChanged(info.mStringInfo[aAttrEnum].mNamespaceID,
                            *info.mStringInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::SMIL);
  }
}

nsSVGElement::StringListAttributesInfo
nsSVGElement::GetStringListInfo()
{
  return StringListAttributesInfo(nullptr, nullptr, 0);
}

nsAttrValue
nsSVGElement::WillChangeStringList(bool aIsConditionalProcessingAttribute,
                                   uint8_t aAttrEnum)
{
  nsIAtom* name;
  if (aIsConditionalProcessingAttribute) {
    nsCOMPtr<SVGTests> tests(do_QueryInterface(static_cast<nsIDOMSVGElement*>(this)));
    name = tests->GetAttrName(aAttrEnum);
  } else {
    name = *GetStringListInfo().mStringListInfo[aAttrEnum].mName;
  }
  return WillChangeValue(name);
}

void
nsSVGElement::DidChangeStringList(bool aIsConditionalProcessingAttribute,
                                  uint8_t aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  nsIAtom* name;
  nsAttrValue newValue;
  nsCOMPtr<SVGTests> tests;

  if (aIsConditionalProcessingAttribute) {
    tests = do_QueryObject(this);
    name = tests->GetAttrName(aAttrEnum);
    tests->GetAttrValue(aAttrEnum, newValue);
  } else {
    StringListAttributesInfo info = GetStringListInfo();

    NS_ASSERTION(info.mStringListCount > 0,
                 "DidChangeStringList on element with no string list attribs");
    NS_ASSERTION(aAttrEnum < info.mStringListCount, "aAttrEnum out of range");

    name = *info.mStringListInfo[aAttrEnum].mName;
    newValue.SetTo(info.mStringLists[aAttrEnum], nullptr);
  }

  DidChangeValue(name, aEmptyOrOldValue, newValue);

  if (aIsConditionalProcessingAttribute) {
    tests->MaybeInvalidate();
  }
}

void
nsSVGElement::StringListAttributesInfo::Reset(uint8_t aAttrEnum)
{
  mStringLists[aAttrEnum].Clear();
  // caller notifies
}

nsresult
nsSVGElement::ReportAttributeParseFailure(nsIDocument* aDocument,
                                          nsIAtom* aAttribute,
                                          const nsAString& aValue)
{
  const nsString& attributeValue = PromiseFlatString(aValue);
  const char16_t *strings[] = { aAttribute->GetUTF16String(),
                                 attributeValue.get() };
  return SVGContentUtils::ReportToConsole(aDocument,
                                          "AttributeParseWarning",
                                          strings, ArrayLength(strings));
}

void
nsSVGElement::RecompileScriptEventListeners()
{
  int32_t i, count = mAttrsAndChildren.AttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);

    // Eventlistenener-attributes are always in the null namespace
    if (!name->IsAtom()) {
        continue;
    }

    nsIAtom *attr = name->Atom();
    if (!IsEventAttributeName(attr)) {
      continue;
    }

    nsAutoString value;
    GetAttr(kNameSpaceID_None, attr, value);
    SetEventHandler(GetEventNameForAttr(attr), value, true);
  }
}

UniquePtr<nsISMILAttr>
nsSVGElement::GetAnimatedAttr(int32_t aNamespaceID, nsIAtom* aName)
{
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
    for (uint32_t i = 0; i < info.mLengthCount; i++) {
      if (aName == *info.mLengthInfo[i].mName) {
        return info.mLengths[i].ToSMILAttr(this);
      }
    }

    // Numbers:
    {
      NumberAttributesInfo info = GetNumberInfo();
      for (uint32_t i = 0; i < info.mNumberCount; i++) {
        if (aName == *info.mNumberInfo[i].mName) {
          return info.mNumbers[i].ToSMILAttr(this);
        }
      }
    }

    // Number Pairs:
    {
      NumberPairAttributesInfo info = GetNumberPairInfo();
      for (uint32_t i = 0; i < info.mNumberPairCount; i++) {
        if (aName == *info.mNumberPairInfo[i].mName) {
          return info.mNumberPairs[i].ToSMILAttr(this);
        }
      }
    }

    // Integers:
    {
      IntegerAttributesInfo info = GetIntegerInfo();
      for (uint32_t i = 0; i < info.mIntegerCount; i++) {
        if (aName == *info.mIntegerInfo[i].mName) {
          return info.mIntegers[i].ToSMILAttr(this);
        }
      }
    }

    // Integer Pairs:
    {
      IntegerPairAttributesInfo info = GetIntegerPairInfo();
      for (uint32_t i = 0; i < info.mIntegerPairCount; i++) {
        if (aName == *info.mIntegerPairInfo[i].mName) {
          return info.mIntegerPairs[i].ToSMILAttr(this);
        }
      }
    }

    // Enumerations:
    {
      EnumAttributesInfo info = GetEnumInfo();
      for (uint32_t i = 0; i < info.mEnumCount; i++) {
        if (aName == *info.mEnumInfo[i].mName) {
          return info.mEnums[i].ToSMILAttr(this);
        }
      }
    }

    // Booleans:
    {
      BooleanAttributesInfo info = GetBooleanInfo();
      for (uint32_t i = 0; i < info.mBooleanCount; i++) {
        if (aName == *info.mBooleanInfo[i].mName) {
          return info.mBooleans[i].ToSMILAttr(this);
        }
      }
    }

    // Angles:
    {
      AngleAttributesInfo info = GetAngleInfo();
      for (uint32_t i = 0; i < info.mAngleCount; i++) {
        if (aName == *info.mAngleInfo[i].mName) {
          return info.mAngles[i].ToSMILAttr(this);
        }
      }
    }

    // viewBox:
    if (aName == nsGkAtoms::viewBox) {
      nsSVGViewBox *viewBox = GetViewBox();
      return viewBox ? viewBox->ToSMILAttr(this) : nullptr;
    }

    // preserveAspectRatio:
    if (aName == nsGkAtoms::preserveAspectRatio) {
      SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
        GetPreserveAspectRatio();
      return preserveAspectRatio ?
        preserveAspectRatio->ToSMILAttr(this) : nullptr;
    }

    // NumberLists:
    {
      NumberListAttributesInfo info = GetNumberListInfo();
      for (uint32_t i = 0; i < info.mNumberListCount; i++) {
        if (aName == *info.mNumberListInfo[i].mName) {
          MOZ_ASSERT(i <= UCHAR_MAX, "Too many attributes");
          return info.mNumberLists[i].ToSMILAttr(this, uint8_t(i));
        }
      }
    }

    // LengthLists:
    {
      LengthListAttributesInfo info = GetLengthListInfo();
      for (uint32_t i = 0; i < info.mLengthListCount; i++) {
        if (aName == *info.mLengthListInfo[i].mName) {
          MOZ_ASSERT(i <= UCHAR_MAX, "Too many attributes");
          return info.mLengthLists[i].ToSMILAttr(this,
                                                 uint8_t(i),
                                                 info.mLengthListInfo[i].mAxis,
                                                 info.mLengthListInfo[i].mCouldZeroPadList);
        }
      }
    }

    // PointLists:
    {
      if (GetPointListAttrName() == aName) {
        SVGAnimatedPointList *pointList = GetAnimatedPointList();
        if (pointList) {
          return pointList->ToSMILAttr(this);
        }
      }
    }

    // PathSegLists:
    {
      if (GetPathDataAttrName() == aName) {
        SVGAnimatedPathSegList *segList = GetAnimPathSegList();
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
    for (uint32_t i = 0; i < info.mStringCount; i++) {
      if (aNamespaceID == info.mStringInfo[i].mNamespaceID &&
          aName == *info.mStringInfo[i].mName) {
        return info.mStrings[i].ToSMILAttr(this);
      }
    }
  }

  return nullptr;
}

void
nsSVGElement::AnimationNeedsResample()
{
  nsIDocument* doc = GetComposedDoc();
  if (doc && doc->HasAnimationController()) {
    doc->GetAnimationController()->SetResampleNeeded();
  }
}

void
nsSVGElement::FlushAnimations()
{
  nsIDocument* doc = GetComposedDoc();
  if (doc && doc->HasAnimationController()) {
    doc->GetAnimationController()->FlushResampleRequests();
  }
}
