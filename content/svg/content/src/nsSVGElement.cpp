/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGElement.h"

#include "mozilla/Util.h"

#include "nsSVGSVGElement.h"
#include "nsIDocument.h"
#include "nsRange.h"
#include "nsIDOMAttr.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsXBLPrototypeBinding.h"
#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "mozilla/css/StyleRule.h"
#include "nsRuleWalker.h"
#include "mozilla/css/Declaration.h"
#include "nsCSSProps.h"
#include "nsCSSParser.h"
#include "nsGenericHTMLElement.h"
#include "nsNodeInfoManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsEventListenerManager.h"
#include "nsSVGUtils.h"
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
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedPointList.h"
#include "SVGAnimatedPathSegList.h"
#include "SVGAnimatedTransformList.h"
#include "DOMSVGTests.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGRect.h"
#include "nsIFrame.h"
#include "prdtoa.h"
#include <stdarg.h>
#include "nsSMILMappedAttribute.h"
#include "SVGMotionSMILAttr.h"
#include "nsAttrValueOrString.h"

using namespace mozilla;

// This is needed to ensure correct handling of calls to the
// vararg-list methods in this file:
//   nsSVGElement::GetAnimated{Length,Number,Integer}Values
// See bug 547964 for details:
PR_STATIC_ASSERT(sizeof(void*) == sizeof(nsnull));


nsSVGEnumMapping nsSVGElement::sSVGUnitTypesMap[] = {
  {&nsGkAtoms::userSpaceOnUse, nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE},
  {&nsGkAtoms::objectBoundingBox, nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
  {nsnull, 0}
};

nsSVGElement::nsSVGElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGElementBase(aNodeInfo)
{
}

nsresult
nsSVGElement::Init()
{
  // Set up length attributes - can't do this in the constructor
  // because we can't do a virtual call at that point

  LengthAttributesInfo lengthInfo = GetLengthInfo();

  PRUint32 i;
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

NS_IMPL_ADDREF_INHERITED(nsSVGElement, nsSVGElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGElement, nsSVGElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGElement)
// provided by nsGenericElement:
//  NS_INTERFACE_MAP_ENTRY(nsIContent)
NS_INTERFACE_MAP_END_INHERITING(nsSVGElementBase)

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

  if (oldVal && oldVal->Type() == nsAttrValue::eCSSStyleRule) {
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
    // Don't bother going through SetInlineStyleRule, we don't want to fire off
    // mutation events or document notifications anyway
    rv = mAttrsAndChildren.SetAndTakeAttr(nsGkAtoms::style, attrValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsSVGElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                           const nsAttrValue* aValue, bool aNotify)
{
  // We don't currently use nsMappedAttributes within SVG. If this changes, we
  // need to be very careful because some nsAttrValues used by SVG point to
  // member data of SVG elements and if an nsAttrValue outlives the SVG element
  // whose data it points to (by virtue of being stored in
  // mAttrsAndChildren->mMappedAttributes, meaning it's shared between
  // elements), the pointer will dangle. See bug 724680.
  NS_ABORT_IF_FALSE(!mAttrsAndChildren.HasMappedAttrs(),
    "Unexpected use of nsMappedAttributes within SVG");

  // If this is an svg presentation attribute we need to map it into
  // the content stylerule.
  // XXX For some reason incremental mapping doesn't work, so for now
  // just delete the style rule and lazily reconstruct it in
  // GetContentStyleRule()
  if (aNamespaceID == kNameSpaceID_None && IsAttributeMapped(aName)) {
    mContentStyleRule = nsnull;
  }

  if (IsEventName(aName) && aValue) {
    NS_ABORT_IF_FALSE(aValue->Type() == nsAttrValue::eString,
      "Expected string value for script body");
    nsresult rv = AddScriptEventListener(GetEventNameForAttr(aName),
                                         aValue->GetStringValue());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nsSVGElementBase::AfterSetAttr(aNamespaceID, aName, aValue, aNotify);
}

bool
nsSVGElement::ParseAttribute(PRInt32 aNamespaceID,
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

    PRUint32 i;
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
          nsCOMPtr<nsIAtom> valAtom = do_GetAtom(aValue);
          rv = booleanInfo.mBooleans[i].SetBaseValueAtom(valAtom, this);
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
          nsCOMPtr<nsIAtom> valAtom = do_GetAtom(aValue);
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
      nsCOMPtr<DOMSVGTests> tests(do_QueryInterface(this));
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
          rv = viewBox->SetBaseValueString(aValue, this);
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
          rv = preserveAspectRatio->SetBaseValueString(aValue, this);
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
        SVGAnimatedTransformList *transformList = GetAnimatedTransformList();
        if (transformList) {
          rv = transformList->SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            transformList->ClearBaseValue();
          } else {
            aResult.SetTo(transformList->GetBaseValue(), &aValue);
            didSetResult = true;
          }
          foundMatch = true;
        }
      }
    }
  }

  if (!foundMatch) {
    // Check for nsSVGString attribute
    StringAttributesInfo stringInfo = GetStringInfo();
    for (PRUint32 i = 0; i < stringInfo.mStringCount; i++) {
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
nsSVGElement::UnsetAttrInternal(PRInt32 aNamespaceID, nsIAtom* aName,
                                bool aNotify)
{
  // XXXbz there's a bunch of redundancy here with AfterSetAttr.
  // Maybe consolidate?

  if (aNamespaceID == kNameSpaceID_None) {
    // If this is an svg presentation attribute, remove rule to force an update
    if (IsAttributeMapped(aName))
      mContentStyleRule = nsnull;

    if (IsEventName(aName)) {
      nsEventListenerManager* manager = GetListenerManager(false);
      if (manager) {
        nsIAtom* eventName = GetEventNameForAttr(aName);
        manager->RemoveScriptEventListener(eventName);
      }
      return;
    }
    
    // Check if this is a length attribute going away
    LengthAttributesInfo lenInfo = GetLengthInfo();

    for (PRUint32 i = 0; i < lenInfo.mLengthCount; i++) {
      if (aName == *lenInfo.mLengthInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        lenInfo.Reset(i);
        return;
      }
    }

    // Check if this is a length list attribute going away
    LengthListAttributesInfo lengthListInfo = GetLengthListInfo();

    for (PRUint32 i = 0; i < lengthListInfo.mLengthListCount; i++) {
      if (aName == *lengthListInfo.mLengthListInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        lengthListInfo.Reset(i);
        return;
      }
    }

    // Check if this is a number list attribute going away
    NumberListAttributesInfo numberListInfo = GetNumberListInfo();

    for (PRUint32 i = 0; i < numberListInfo.mNumberListCount; i++) {
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

    for (PRUint32 i = 0; i < numInfo.mNumberCount; i++) {
      if (aName == *numInfo.mNumberInfo[i].mName) {
        numInfo.Reset(i);
        return;
      }
    }

    // Check if this is a number pair attribute going away
    NumberPairAttributesInfo numPairInfo = GetNumberPairInfo();

    for (PRUint32 i = 0; i < numPairInfo.mNumberPairCount; i++) {
      if (aName == *numPairInfo.mNumberPairInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        numPairInfo.Reset(i);
        return;
      }
    }

    // Check if this is an integer attribute going away
    IntegerAttributesInfo intInfo = GetIntegerInfo();

    for (PRUint32 i = 0; i < intInfo.mIntegerCount; i++) {
      if (aName == *intInfo.mIntegerInfo[i].mName) {
        intInfo.Reset(i);
        return;
      }
    }

    // Check if this is an integer pair attribute going away
    IntegerPairAttributesInfo intPairInfo = GetIntegerPairInfo();

    for (PRUint32 i = 0; i < intPairInfo.mIntegerPairCount; i++) {
      if (aName == *intPairInfo.mIntegerPairInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        intPairInfo.Reset(i);
        return;
      }
    }

    // Check if this is an angle attribute going away
    AngleAttributesInfo angleInfo = GetAngleInfo();

    for (PRUint32 i = 0; i < angleInfo.mAngleCount; i++) {
      if (aName == *angleInfo.mAngleInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        angleInfo.Reset(i);
        return;
      }
    }

    // Check if this is a boolean attribute going away
    BooleanAttributesInfo boolInfo = GetBooleanInfo();

    for (PRUint32 i = 0; i < boolInfo.mBooleanCount; i++) {
      if (aName == *boolInfo.mBooleanInfo[i].mName) {
        boolInfo.Reset(i);
        return;
      }
    }

    // Check if this is an enum attribute going away
    EnumAttributesInfo enumInfo = GetEnumInfo();

    for (PRUint32 i = 0; i < enumInfo.mEnumCount; i++) {
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
      SVGAnimatedTransformList *transformList = GetAnimatedTransformList();
      if (transformList) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        transformList->ClearBaseValue();
        return;
      }
    }

    // Check for conditional processing attributes
    nsCOMPtr<DOMSVGTests> tests(do_QueryInterface(this));
    if (tests && tests->IsConditionalProcessingAttribute(aName)) {
      MaybeSerializeAttrBeforeRemoval(aName, aNotify);
      tests->UnsetAttr(aName);
      return;
    }

    // Check if this is a string list attribute going away
    StringListAttributesInfo stringListInfo = GetStringListInfo();

    for (PRUint32 i = 0; i < stringListInfo.mStringListCount; i++) {
      if (aName == *stringListInfo.mStringListInfo[i].mName) {
        MaybeSerializeAttrBeforeRemoval(aName, aNotify);
        stringListInfo.Reset(i);
        return;
      }
    }
  }

  // Check if this is a string attribute going away
  StringAttributesInfo stringInfo = GetStringInfo();

  for (PRUint32 i = 0; i < stringInfo.mStringCount; i++) {
    if (aNamespaceID == stringInfo.mStringInfo[i].mNamespaceID &&
        aName == *stringInfo.mStringInfo[i].mName) {
      stringInfo.Reset(i);
      return;
    }
  }
}

nsresult
nsSVGElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                        bool aNotify)
{
  UnsetAttrInternal(aNamespaceID, aName, aNotify);
  return nsSVGElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

nsChangeHint
nsSVGElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                     PRInt32 aModType) const
{
  nsChangeHint retval =
    nsSVGElementBase::GetAttributeChangeHint(aAttribute, aModType);

  nsCOMPtr<DOMSVGTests> tests(do_QueryInterface(const_cast<nsSVGElement*>(this)));
  if (tests && tests->IsConditionalProcessingAttribute(aAttribute)) {
    // It would be nice to only reconstruct the frame if the value returned by
    // DOMSVGTests::PassesConditionalProcessingTests has changed, but we don't
    // know that
    NS_UpdateHint(retval, nsChangeHint_ReconstructFrame);
  }
  return retval;
}

bool
nsSVGElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~eCONTENT);
}

NS_IMETHODIMP
nsSVGElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
#ifdef DEBUG
//  printf("nsSVGElement(%p)::WalkContentStyleRules()\n", this);
#endif
  if (!mContentStyleRule)
    UpdateContentStyleRule();

  if (mContentStyleRule) {
    mContentStyleRule->RuleMatched();
    aRuleWalker->Forward(mContentStyleRule);
  }

  // Update & walk the animated content style rule, to include style from
  // animated mapped attributes.  But first, get nsPresContext to check
  // whether this is a "no-animation restyle". (This should match the check
  // in nsHTMLCSSStyleSheet::RulesMatching(), where we determine whether to
  // apply the SMILOverrideStyle.)
  nsIDocument* doc = OwnerDoc();
  nsIPresShell* shell = doc->GetShell();
  nsPresContext* context = shell ? shell->GetPresContext() : nsnull;
  if (context && context->IsProcessingRestyles() &&
      !context->IsProcessingAnimationStyleChange()) {
    // Any style changes right now could trigger CSS Transitions. We don't
    // want that to happen from SMIL-animated value of mapped attrs, so
    // ignore animated value for now, and request an animation restyle to
    // get our animated value noticed.
    shell->RestyleForAnimation(this, eRestyle_Self);
  } else {
    // Ok, this is an animation restyle -- go ahead and update/walk the
    // animated content style rule.
    css::StyleRule* animContentStyleRule = GetAnimatedContentStyleRule();
    if (!animContentStyleRule) {
      UpdateAnimatedContentStyleRule();
      animContentStyleRule = GetAnimatedContentStyleRule();
    }
    if (animContentStyleRule) {
      animContentStyleRule->RuleMatched();
      aRuleWalker->Forward(animContentStyleRule);
    }
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
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFillStrokeMap[] = {
  { &nsGkAtoms::fill },
  { &nsGkAtoms::fill_opacity },
  { &nsGkAtoms::fill_rule },
  { &nsGkAtoms::stroke },
  { &nsGkAtoms::stroke_dasharray },
  { &nsGkAtoms::stroke_dashoffset },
  { &nsGkAtoms::stroke_linecap },
  { &nsGkAtoms::stroke_linejoin },
  { &nsGkAtoms::stroke_miterlimit },
  { &nsGkAtoms::stroke_opacity },
  { &nsGkAtoms::stroke_width },
  { &nsGkAtoms::vector_effect },
  { nsnull }
};

// PresentationAttributes-Graphics
/* static */ const nsGenericElement::MappedAttributeEntry
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
  { nsnull }
};

// PresentationAttributes-TextContentElements
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sTextContentElementsMap[] = {
  { &nsGkAtoms::alignment_baseline },
  { &nsGkAtoms::baseline_shift },
  { &nsGkAtoms::direction },
  { &nsGkAtoms::dominant_baseline },
  { &nsGkAtoms::glyph_orientation_horizontal },
  { &nsGkAtoms::glyph_orientation_vertical },
  { &nsGkAtoms::kerning },
  { &nsGkAtoms::letter_spacing },
  { &nsGkAtoms::text_anchor },
  { &nsGkAtoms::text_decoration },
  { &nsGkAtoms::unicode_bidi },
  { &nsGkAtoms::word_spacing },
  { nsnull }
};

// PresentationAttributes-FontSpecification
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFontSpecificationMap[] = {
  { &nsGkAtoms::font_family },
  { &nsGkAtoms::font_size },
  { &nsGkAtoms::font_size_adjust },
  { &nsGkAtoms::font_stretch },
  { &nsGkAtoms::font_style },
  { &nsGkAtoms::font_variant },
  { &nsGkAtoms::fontWeight },  
  { nsnull }
};

// PresentationAttributes-GradientStop
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sGradientStopMap[] = {
  { &nsGkAtoms::stop_color },
  { &nsGkAtoms::stop_opacity },
  { nsnull }
};

// PresentationAttributes-Viewports
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sViewportsMap[] = {
  { &nsGkAtoms::overflow },
  { &nsGkAtoms::clip },
  { nsnull }
};

// PresentationAttributes-Makers
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sMarkersMap[] = {
  { &nsGkAtoms::marker_end },
  { &nsGkAtoms::marker_mid },
  { &nsGkAtoms::marker_start },
  { nsnull }
};

// PresentationAttributes-Color
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sColorMap[] = {
  { &nsGkAtoms::color },
  { nsnull }
};

// PresentationAttributes-Filters
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFiltersMap[] = {
  { &nsGkAtoms::colorInterpolationFilters },
  { nsnull }
};

// PresentationAttributes-feFlood
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFEFloodMap[] = {
  { &nsGkAtoms::flood_color },
  { &nsGkAtoms::flood_opacity },
  { nsnull }
};

// PresentationAttributes-LightingEffects
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sLightingEffectsMap[] = {
  { &nsGkAtoms::lighting_color },
  { nsnull }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMETHODIMP
nsSVGElement::IsSupported(const nsAString& aFeature, const nsAString& aVersion, bool* aReturn)
{
  return nsGenericElement::IsSupported(aFeature, aVersion, aReturn); 
}

//----------------------------------------------------------------------
// nsIDOMElement methods

// forwarded to nsGenericElement implementations


//----------------------------------------------------------------------
// nsIDOMSVGElement methods

/* attribute DOMString id; */
NS_IMETHODIMP nsSVGElement::GetId(nsAString & aId)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::id, aId);

  return NS_OK;
}

NS_IMETHODIMP nsSVGElement::SetId(const nsAString & aId)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::id, aId, true);
}

/* readonly attribute nsIDOMSVGSVGElement ownerSVGElement; */
NS_IMETHODIMP
nsSVGElement::GetOwnerSVGElement(nsIDOMSVGSVGElement * *aOwnerSVGElement)
{
  NS_IF_ADDREF(*aOwnerSVGElement = GetCtx());

  if (*aOwnerSVGElement || Tag() == nsGkAtoms::svg) {
    // If we found something or we're the outermost SVG element, that's OK.
    return NS_OK;
  }
  // Otherwise, we've got an invalid structure
  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIDOMSVGElement viewportElement; */
NS_IMETHODIMP
nsSVGElement::GetViewportElement(nsIDOMSVGElement * *aViewportElement)
{
  *aViewportElement = nsSVGUtils::GetNearestViewportElement(this).get();
  return NS_OK;
}

//------------------------------------------------------------------------
// Helper class: MappedAttrParser, for parsing values of mapped attributes

namespace {

class MappedAttrParser {
public:
  MappedAttrParser(css::Loader* aLoader,
                   nsIURI* aDocURI,
                   already_AddRefed<nsIURI> aBaseURI,
                   nsIPrincipal* aNodePrincipal);
  ~MappedAttrParser();

  // Parses a mapped attribute value.
  void ParseMappedAttrValue(nsIAtom* aMappedAttrName,
                            nsAString& aMappedAttrValue);

  // If we've parsed any values for mapped attributes, this method returns
  // a new already_AddRefed css::StyleRule that incorporates the parsed
  // values. Otherwise, this method returns null.
  already_AddRefed<css::StyleRule> CreateStyleRule();

private:
  // MEMBER DATA
  // -----------
  nsCSSParser       mParser;

  // Arguments for nsCSSParser::ParseProperty
  nsIURI*           mDocURI;
  nsCOMPtr<nsIURI>  mBaseURI;
  nsIPrincipal*     mNodePrincipal;

  // Declaration for storing parsed values (lazily initialized)
  css::Declaration* mDecl;
};

MappedAttrParser::MappedAttrParser(css::Loader* aLoader,
                                   nsIURI* aDocURI,
                                   already_AddRefed<nsIURI> aBaseURI,
                                   nsIPrincipal* aNodePrincipal)
  : mParser(aLoader), mDocURI(aDocURI), mBaseURI(aBaseURI),
    mNodePrincipal(aNodePrincipal), mDecl(nsnull)
{
  // SVG and CSS differ slightly in their interpretation of some of
  // the attributes.  SVG allows attributes of the form: font-size="5"
  // (style="font-size: 5" if using a style attribute)
  // where CSS requires units: font-size="5pt" (style="font-size: 5pt")
  // Set a flag to pass information to the parser so that we can use
  // the CSS parser to parse the font-size attribute.  Note that this
  // does *not* affect the use of CSS stylesheets, which will still
  // require units.
  mParser.SetSVGMode(true);
}

MappedAttrParser::~MappedAttrParser()
{
  NS_ABORT_IF_FALSE(!mDecl,
                    "If mDecl was initialized, it should have been converted "
                    "into a style rule (and had its pointer cleared)");
}

void
MappedAttrParser::ParseMappedAttrValue(nsIAtom* aMappedAttrName,
                                       nsAString& aMappedAttrValue)
{
  if (!mDecl) {
    mDecl = new css::Declaration();
    mDecl->InitializeEmpty();
  }

  // Get the nsCSSProperty ID for our mapped attribute.
  nsCSSProperty propertyID =
    nsCSSProps::LookupProperty(nsDependentAtomString(aMappedAttrName));
  if (propertyID != eCSSProperty_UNKNOWN) {
    bool changed; // outparam for ParseProperty. (ignored)
    mParser.ParseProperty(propertyID, aMappedAttrValue, mDocURI, mBaseURI,
                          mNodePrincipal, mDecl, &changed, false);
    return;
  }
  NS_ABORT_IF_FALSE(aMappedAttrName == nsGkAtoms::lang,
                    "Only 'lang' should be unrecognized!");
  // nsCSSParser doesn't know about 'lang', so we need to handle it specially.
  if (aMappedAttrName == nsGkAtoms::lang) {
    propertyID = eCSSProperty__x_lang;
    nsCSSExpandedDataBlock block;
    mDecl->ExpandTo(&block);
    nsCSSValue cssValue(PromiseFlatString(aMappedAttrValue), eCSSUnit_Ident);
    block.AddLonghandProperty(propertyID, cssValue);
    mDecl->ValueAppended(propertyID);
    mDecl->CompressFrom(&block);
  }
}

already_AddRefed<css::StyleRule>
MappedAttrParser::CreateStyleRule()
{
  if (!mDecl) {
    return nsnull; // No mapped attributes were parsed
  }

  nsRefPtr<css::StyleRule> rule = new css::StyleRule(nsnull, mDecl);
  mDecl = nsnull; // We no longer own the declaration -- drop our pointer to it
  return rule.forget();
}

} // anonymous namespace

//----------------------------------------------------------------------
// Implementation Helpers:

bool
nsSVGElement::IsEventName(nsIAtom* aName)
{
  return false;
}

void
nsSVGElement::UpdateContentStyleRule()
{
  NS_ASSERTION(!mContentStyleRule, "we already have a content style rule");

  PRUint32 attrCount = mAttrsAndChildren.AttrCount();
  if (!attrCount) {
    // nothing to do
    return;
  }

  nsIDocument* doc = OwnerDoc();
  MappedAttrParser mappedAttrParser(doc->CSSLoader(), doc->GetDocumentURI(),
                                    GetBaseURI(), NodePrincipal());

  for (PRUint32 i = 0; i < attrCount; ++i) {
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

    if (Tag() == nsGkAtoms::svg) {
      // Special case: we don't want <svg> 'width'/'height' mapped into style
      // if the attribute value isn't a valid <length> according to SVG (which
      // only supports a subset of the CSS <length> values). We don't enforce
      // this by checking the attribute value in nsSVGSVGElement::
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
  mContentStyleRule = mappedAttrParser.CreateStyleRule();
}

static void
ParseMappedAttrAnimValueCallback(void*    aObject,
                                 nsIAtom* aPropertyName,
                                 void*    aPropertyValue,
                                 void*    aData)
{
  NS_ABORT_IF_FALSE(aPropertyName != SMIL_MAPPED_ATTR_STYLERULE_ATOM,
                    "animated content style rule should have been removed "
                    "from properties table already (we're rebuilding it now)");

  MappedAttrParser* mappedAttrParser =
    static_cast<MappedAttrParser*>(aData);

  nsStringBuffer* valueBuf = static_cast<nsStringBuffer*>(aPropertyValue);
  nsAutoString value;
  PRUint32 len = NS_strlen(static_cast<PRUnichar*>(valueBuf->Data()));
  valueBuf->ToString(len, value);

  mappedAttrParser->ParseMappedAttrValue(aPropertyName, value);
}

// Callback for freeing animated content style rule, in property table.
static void
ReleaseStyleRule(void*    aObject,       /* unused */
                 nsIAtom* aPropertyName,
                 void*    aPropertyValue,
                 void*    aData          /* unused */)
{
  NS_ABORT_IF_FALSE(aPropertyName == SMIL_MAPPED_ATTR_STYLERULE_ATOM,
                    "unexpected property name, for "
                    "animated content style rule");
  css::StyleRule* styleRule = static_cast<css::StyleRule*>(aPropertyValue);
  NS_ABORT_IF_FALSE(styleRule, "unexpected null style rule");
  styleRule->Release();
}

void
nsSVGElement::UpdateAnimatedContentStyleRule()
{
  NS_ABORT_IF_FALSE(!GetAnimatedContentStyleRule(),
                    "Animated content style rule already set");

  nsIDocument* doc = OwnerDoc();
  if (!doc) {
    NS_ERROR("SVG element without owner document");
    return;
  }

  MappedAttrParser mappedAttrParser(doc->CSSLoader(), doc->GetDocumentURI(),
                                    GetBaseURI(), NodePrincipal());
  doc->PropertyTable(SMIL_MAPPED_ATTR_ANIMVAL)->
    Enumerate(this, ParseMappedAttrAnimValueCallback, &mappedAttrParser);
 
  nsRefPtr<css::StyleRule>
    animContentStyleRule(mappedAttrParser.CreateStyleRule());

  if (animContentStyleRule) {
#ifdef DEBUG
    nsresult rv =
#endif
      SetProperty(SMIL_MAPPED_ATTR_ANIMVAL,
                  SMIL_MAPPED_ATTR_STYLERULE_ATOM,
                  animContentStyleRule.get(),
                  ReleaseStyleRule);
    animContentStyleRule.forget();
    NS_ABORT_IF_FALSE(rv == NS_OK,
                      "SetProperty failed (or overwrote something)");
  }
}

css::StyleRule*
nsSVGElement::GetAnimatedContentStyleRule()
{
  return
    static_cast<css::StyleRule*>(GetProperty(SMIL_MAPPED_ATTR_ANIMVAL,
                                             SMIL_MAPPED_ATTR_STYLERULE_ATOM,
                                             nsnull));
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
  //   a) to pass to BeforeSetAttr when GetParsedAttr returns nsnull
  //   b) to store the old value in the case we have mutation listeners
  // We can use the same value for both purposes since (a) happens before (b).
  // Also, we should be careful to always return this value to benefit from
  // return value optimization.
  nsAttrValue emptyOrOldAttrValue;
  const nsAttrValue* attrValue = GetParsedAttr(aName);

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
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "Unexpected failure from BeforeSetAttr");

  // We only need to set the old value if we have listeners since otherwise it
  // isn't used.
  if (attrValue &&
      nsContentUtils::HasMutationListeners(this,
                                           NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                           this)) {
    emptyOrOldAttrValue.SetToSerialized(*attrValue);
  }

  PRUint8 modType = attrValue
                  ? static_cast<PRUint8>(nsIDOMMutationEvent::MODIFICATION)
                  : static_cast<PRUint8>(nsIDOMMutationEvent::ADDITION);
  nsNodeUtils::AttributeWillChange(this, kNameSpaceID_None, aName, modType);

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
  PRUint8 modType = HasAttr(kNameSpaceID_None, aName)
                  ? static_cast<PRUint8>(nsIDOMMutationEvent::MODIFICATION)
                  : static_cast<PRUint8>(nsIDOMMutationEvent::ADDITION);
  SetAttrAndNotify(kNameSpaceID_None, aName, nsnull, aEmptyOrOldValue,
                   aNewValue, modType, hasListeners, kNotifyDocumentObservers,
                   kCallAfterSetAttr);
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
  mAttrsAndChildren.SetAndTakeAttr(aName, oldAttrValue);
}

/* static */
nsIAtom* nsSVGElement::GetEventNameForAttr(nsIAtom* aAttr)
{
  if (aAttr == nsGkAtoms::onload)
    return nsGkAtoms::onSVGLoad;
  if (aAttr == nsGkAtoms::onunload)
    return nsGkAtoms::onSVGUnload;
  if (aAttr == nsGkAtoms::onabort)
    return nsGkAtoms::onSVGAbort;
  if (aAttr == nsGkAtoms::onerror)
    return nsGkAtoms::onSVGError;
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

nsSVGSVGElement *
nsSVGElement::GetCtx() const
{
  nsIContent* ancestor = GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVG()) {
    nsIAtom* tag = ancestor->Tag();
    if (tag == nsGkAtoms::foreignObject) {
      return nsnull;
    }
    if (tag == nsGkAtoms::svg) {
      return static_cast<nsSVGSVGElement*>(ancestor);
    }
    ancestor = ancestor->GetFlattenedTreeParent();
  }

  // we don't have an ancestor <svg> element...
  return nsnull;
}

/* virtual */ gfxMatrix
nsSVGElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                       TransformTypes aWhich) const
{
  return aMatrix;
}

nsSVGElement::LengthAttributesInfo
nsSVGElement::GetLengthInfo()
{
  return LengthAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::LengthAttributesInfo::Reset(PRUint8 aAttrEnum)
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

  for (PRUint32 i = 0; i < lengthInfo.mLengthCount; i++) {
    if (aName == *lengthInfo.mLengthInfo[i].mName) {
      lengthInfo.mLengths[i] = aLength;
      DidAnimateLength(i);
      return;
    }
  }
  NS_ABORT_IF_FALSE(false, "no length found to set");
}

nsAttrValue
nsSVGElement::WillChangeLength(PRUint8 aAttrEnum)
{
  return WillChangeValue(*GetLengthInfo().mLengthInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeLength(PRUint8 aAttrEnum,
                              const nsAttrValue& aEmptyOrOldValue)
{
  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mLengthCount > 0,
               "DidChangeLength on element with no length attribs");
  NS_ASSERTION(aAttrEnum < info.mLengthCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mLengths[aAttrEnum], nsnull);

  DidChangeValue(*info.mLengthInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateLength(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    LengthAttributesInfo info = GetLengthInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mLengthInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGLength2*
nsSVGElement::GetAnimatedLength(const nsIAtom *aAttrName)
{
  LengthAttributesInfo lengthInfo = GetLengthInfo();

  for (PRUint32 i = 0; i < lengthInfo.mLengthCount; i++) {
    if (aAttrName == *lengthInfo.mLengthInfo[i].mName) {
      return &lengthInfo.mLengths[i];
    }
  }
  NS_ABORT_IF_FALSE(false, "no matching length found");
  return nsnull;
}

void
nsSVGElement::GetAnimatedLengthValues(float *aFirst, ...)
{
  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mLengthCount > 0,
               "GetAnimatedLengthValues on element with no length attribs");

  nsSVGSVGElement *ctx = nsnull;

  float *f = aFirst;
  PRUint32 i = 0;

  va_list args;
  va_start(args, aFirst);

  while (f && i < info.mLengthCount) {
    PRUint8 type = info.mLengths[i].GetSpecifiedUnitType();
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
  return LengthListAttributesInfo(nsnull, nsnull, 0);
}

void
nsSVGElement::LengthListAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mLengthLists[aAttrEnum].ClearBaseValue(aAttrEnum);
  // caller notifies
}

nsAttrValue
nsSVGElement::WillChangeLengthList(PRUint8 aAttrEnum)
{
  return WillChangeValue(*GetLengthListInfo().mLengthListInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeLengthList(PRUint8 aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(info.mLengthListCount > 0,
               "DidChangeLengthList on element with no length list attribs");
  NS_ASSERTION(aAttrEnum < info.mLengthListCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mLengthLists[aAttrEnum].GetBaseValue(), nsnull);

  DidChangeValue(*info.mLengthListInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateLengthList(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    LengthListAttributesInfo info = GetLengthListInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mLengthListInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

void
nsSVGElement::GetAnimatedLengthListValues(SVGUserUnitList *aFirst, ...)
{
  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(info.mLengthListCount > 0,
               "GetAnimatedLengthListValues on element with no length list attribs");

  SVGUserUnitList *list = aFirst;
  PRUint32 i = 0;

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
nsSVGElement::GetAnimatedLengthList(PRUint8 aAttrEnum)
{
  LengthListAttributesInfo info = GetLengthListInfo();
  if (aAttrEnum < info.mLengthListCount) {
    return &(info.mLengthLists[aAttrEnum]);
  }
  NS_NOTREACHED("Bad attrEnum");
  return nsnull;
}


nsSVGElement::NumberListAttributesInfo
nsSVGElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(nsnull, nsnull, 0);
}

void
nsSVGElement::NumberListAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  NS_ABORT_IF_FALSE(aAttrEnum < mNumberListCount, "Bad attr enum");
  mNumberLists[aAttrEnum].ClearBaseValue(aAttrEnum);
  // caller notifies
}

nsAttrValue
nsSVGElement::WillChangeNumberList(PRUint8 aAttrEnum)
{
  return WillChangeValue(*GetNumberListInfo().mNumberListInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeNumberList(PRUint8 aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  NumberListAttributesInfo info = GetNumberListInfo();

  NS_ABORT_IF_FALSE(info.mNumberListCount > 0,
    "DidChangeNumberList on element with no number list attribs");
  NS_ABORT_IF_FALSE(aAttrEnum < info.mNumberListCount,
    "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mNumberLists[aAttrEnum].GetBaseValue(), nsnull);

  DidChangeValue(*info.mNumberListInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateNumberList(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    NumberListAttributesInfo info = GetNumberListInfo();
    NS_ABORT_IF_FALSE(aAttrEnum < info.mNumberListCount, "aAttrEnum out of range");

    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mNumberListInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

SVGAnimatedNumberList*
nsSVGElement::GetAnimatedNumberList(PRUint8 aAttrEnum)
{
  NumberListAttributesInfo info = GetNumberListInfo();
  if (aAttrEnum < info.mNumberListCount) {
    return &(info.mNumberLists[aAttrEnum]);
  }
  NS_ABORT_IF_FALSE(false, "Bad attrEnum");
  return nsnull;
}

SVGAnimatedNumberList*
nsSVGElement::GetAnimatedNumberList(nsIAtom *aAttrName)
{
  NumberListAttributesInfo info = GetNumberListInfo();
  for (PRUint32 i = 0; i < info.mNumberListCount; i++) {
    if (aAttrName == *info.mNumberListInfo[i].mName) {
      return &info.mNumberLists[i];
    }
  }
  NS_ABORT_IF_FALSE(false, "Bad caller");
  return nsnull;
}

nsAttrValue
nsSVGElement::WillChangePointList()
{
  NS_ABORT_IF_FALSE(GetPointListAttrName(),
                    "Changing non-existent point list?");
  return WillChangeValue(GetPointListAttrName());
}

void
nsSVGElement::DidChangePointList(const nsAttrValue& aEmptyOrOldValue)
{
  NS_ABORT_IF_FALSE(GetPointListAttrName(),
                    "Changing non-existent point list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimatedPointList()->GetBaseValue(), nsnull);

  DidChangeValue(GetPointListAttrName(), aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimatePointList()
{
  NS_ABORT_IF_FALSE(GetPointListAttrName(),
                    "Animating non-existent path data?");

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            GetPointListAttrName(),
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsAttrValue
nsSVGElement::WillChangePathSegList()
{
  NS_ABORT_IF_FALSE(GetPathDataAttrName(),
                    "Changing non-existent path seg list?");
  return WillChangeValue(GetPathDataAttrName());
}

void
nsSVGElement::DidChangePathSegList(const nsAttrValue& aEmptyOrOldValue)
{
  NS_ABORT_IF_FALSE(GetPathDataAttrName(),
                    "Changing non-existent path seg list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimPathSegList()->GetBaseValue(), nsnull);

  DidChangeValue(GetPathDataAttrName(), aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimatePathSegList()
{
  NS_ABORT_IF_FALSE(GetPathDataAttrName(),
                    "Animating non-existent path data?");

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            GetPathDataAttrName(),
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::NumberAttributesInfo
nsSVGElement::GetNumberInfo()
{
  return NumberAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::NumberAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mNumbers[aAttrEnum].Init(aAttrEnum,
                           mNumberInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeNumber(PRUint8 aAttrEnum)
{
  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mNumberCount > 0,
               "DidChangeNumber on element with no number attribs");
  NS_ASSERTION(aAttrEnum < info.mNumberCount, "aAttrEnum out of range");

  nsAttrValue attrValue;
  attrValue.SetTo(info.mNumbers[aAttrEnum].GetBaseValue(), nsnull);

  SetParsedAttr(kNameSpaceID_None, *info.mNumberInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
}

void
nsSVGElement::DidAnimateNumber(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    NumberAttributesInfo info = GetNumberInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mNumberInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

void
nsSVGElement::GetAnimatedNumberValues(float *aFirst, ...)
{
  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mNumberCount > 0,
               "GetAnimatedNumberValues on element with no number attribs");

  float *f = aFirst;
  PRUint32 i = 0;

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
  return NumberPairAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::NumberPairAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mNumberPairs[aAttrEnum].Init(aAttrEnum,
                               mNumberPairInfo[aAttrEnum].mDefaultValue1,
                               mNumberPairInfo[aAttrEnum].mDefaultValue2);
}

nsAttrValue
nsSVGElement::WillChangeNumberPair(PRUint8 aAttrEnum)
{
  return WillChangeValue(*GetNumberPairInfo().mNumberPairInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeNumberPair(PRUint8 aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  NumberPairAttributesInfo info = GetNumberPairInfo();

  NS_ASSERTION(info.mNumberPairCount > 0,
               "DidChangePairNumber on element with no number pair attribs");
  NS_ASSERTION(aAttrEnum < info.mNumberPairCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mNumberPairs[aAttrEnum], nsnull);

  DidChangeValue(*info.mNumberPairInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateNumberPair(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    NumberPairAttributesInfo info = GetNumberPairInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mNumberPairInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::IntegerAttributesInfo
nsSVGElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::IntegerAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mIntegers[aAttrEnum].Init(aAttrEnum,
                            mIntegerInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeInteger(PRUint8 aAttrEnum)
{
  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mIntegerCount > 0,
               "DidChangeInteger on element with no integer attribs");
  NS_ASSERTION(aAttrEnum < info.mIntegerCount, "aAttrEnum out of range");

  nsAttrValue attrValue;
  attrValue.SetTo(info.mIntegers[aAttrEnum].GetBaseValue(), nsnull);

  SetParsedAttr(kNameSpaceID_None, *info.mIntegerInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
}

void
nsSVGElement::DidAnimateInteger(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    IntegerAttributesInfo info = GetIntegerInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mIntegerInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

void
nsSVGElement::GetAnimatedIntegerValues(PRInt32 *aFirst, ...)
{
  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mIntegerCount > 0,
               "GetAnimatedIntegerValues on element with no integer attribs");

  PRInt32 *n = aFirst;
  PRUint32 i = 0;

  va_list args;
  va_start(args, aFirst);

  while (n && i < info.mIntegerCount) {
    *n = info.mIntegers[i++].GetAnimValue();
    n = va_arg(args, PRInt32*);
  }
  va_end(args);
}

nsSVGElement::IntegerPairAttributesInfo
nsSVGElement::GetIntegerPairInfo()
{
  return IntegerPairAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::IntegerPairAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mIntegerPairs[aAttrEnum].Init(aAttrEnum,
                                mIntegerPairInfo[aAttrEnum].mDefaultValue1,
                                mIntegerPairInfo[aAttrEnum].mDefaultValue2);
}

nsAttrValue
nsSVGElement::WillChangeIntegerPair(PRUint8 aAttrEnum)
{
  return WillChangeValue(
    *GetIntegerPairInfo().mIntegerPairInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeIntegerPair(PRUint8 aAttrEnum,
                                   const nsAttrValue& aEmptyOrOldValue)
{
  IntegerPairAttributesInfo info = GetIntegerPairInfo();

  NS_ASSERTION(info.mIntegerPairCount > 0,
               "DidChangeIntegerPair on element with no integer pair attribs");
  NS_ASSERTION(aAttrEnum < info.mIntegerPairCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mIntegerPairs[aAttrEnum], nsnull);

  DidChangeValue(*info.mIntegerPairInfo[aAttrEnum].mName, aEmptyOrOldValue,
                 newValue);
}

void
nsSVGElement::DidAnimateIntegerPair(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    IntegerPairAttributesInfo info = GetIntegerPairInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mIntegerPairInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::AngleAttributesInfo
nsSVGElement::GetAngleInfo()
{
  return AngleAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::AngleAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mAngles[aAttrEnum].Init(aAttrEnum, 
                          mAngleInfo[aAttrEnum].mDefaultValue,
                          mAngleInfo[aAttrEnum].mDefaultUnitType);
}

nsAttrValue
nsSVGElement::WillChangeAngle(PRUint8 aAttrEnum)
{
  return WillChangeValue(*GetAngleInfo().mAngleInfo[aAttrEnum].mName);
}

void
nsSVGElement::DidChangeAngle(PRUint8 aAttrEnum,
                             const nsAttrValue& aEmptyOrOldValue)
{
  AngleAttributesInfo info = GetAngleInfo();

  NS_ASSERTION(info.mAngleCount > 0,
               "DidChangeAngle on element with no angle attribs");
  NS_ASSERTION(aAttrEnum < info.mAngleCount, "aAttrEnum out of range");

  nsAttrValue newValue;
  newValue.SetTo(info.mAngles[aAttrEnum], nsnull);

  DidChangeValue(*info.mAngleInfo[aAttrEnum].mName, aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimateAngle(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    AngleAttributesInfo info = GetAngleInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mAngleInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::BooleanAttributesInfo
nsSVGElement::GetBooleanInfo()
{
  return BooleanAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::BooleanAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mBooleans[aAttrEnum].Init(aAttrEnum,
                            mBooleanInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeBoolean(PRUint8 aAttrEnum)
{
  BooleanAttributesInfo info = GetBooleanInfo();

  NS_ASSERTION(info.mBooleanCount > 0,
               "DidChangeBoolean on element with no boolean attribs");
  NS_ASSERTION(aAttrEnum < info.mBooleanCount, "aAttrEnum out of range");

  nsAttrValue attrValue(info.mBooleans[aAttrEnum].GetBaseValueAtom());
  SetParsedAttr(kNameSpaceID_None, *info.mBooleanInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
}

void
nsSVGElement::DidAnimateBoolean(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    BooleanAttributesInfo info = GetBooleanInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mBooleanInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::EnumAttributesInfo
nsSVGElement::GetEnumInfo()
{
  return EnumAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::EnumAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mEnums[aAttrEnum].Init(aAttrEnum,
                         mEnumInfo[aAttrEnum].mDefaultValue);
}

void
nsSVGElement::DidChangeEnum(PRUint8 aAttrEnum)
{
  EnumAttributesInfo info = GetEnumInfo();

  NS_ASSERTION(info.mEnumCount > 0,
               "DidChangeEnum on element with no enum attribs");
  NS_ASSERTION(aAttrEnum < info.mEnumCount, "aAttrEnum out of range");

  nsAttrValue attrValue(info.mEnums[aAttrEnum].GetBaseValueAtom(this));
  SetParsedAttr(kNameSpaceID_None, *info.mEnumInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
}

void
nsSVGElement::DidAnimateEnum(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    EnumAttributesInfo info = GetEnumInfo();
    frame->AttributeChanged(kNameSpaceID_None,
                            *info.mEnumInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGViewBox *
nsSVGElement::GetViewBox()
{
  return nsnull;
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
  newValue.SetTo(*viewBox, nsnull);

  DidChangeValue(nsGkAtoms::viewBox, aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimateViewBox()
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            nsGkAtoms::viewBox,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

SVGAnimatedPreserveAspectRatio *
nsSVGElement::GetPreserveAspectRatio()
{
  return nsnull;
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
  newValue.SetTo(*preserveAspectRatio, nsnull);

  DidChangeValue(nsGkAtoms::preserveAspectRatio, aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimatePreserveAspectRatio()
{
  nsIFrame* frame = GetPrimaryFrame();
  
  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            nsGkAtoms::preserveAspectRatio,
                            nsIDOMMutationEvent::MODIFICATION);
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
  NS_ABORT_IF_FALSE(GetTransformListAttrName(),
                    "Changing non-existent transform list?");

  nsAttrValue newValue;
  newValue.SetTo(GetAnimatedTransformList()->GetBaseValue(), nsnull);

  DidChangeValue(GetTransformListAttrName(), aEmptyOrOldValue, newValue);
}

void
nsSVGElement::DidAnimateTransformList()
{
  NS_ABORT_IF_FALSE(GetTransformListAttrName(),
                    "Animating non-existent transform data?");

  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    frame->AttributeChanged(kNameSpaceID_None,
                            GetTransformListAttrName(),
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::StringAttributesInfo
nsSVGElement::GetStringInfo()
{
  return StringAttributesInfo(nsnull, nsnull, 0);
}

void nsSVGElement::StringAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mStrings[aAttrEnum].Init(aAttrEnum);
}

void nsSVGElement::GetStringBaseValue(PRUint8 aAttrEnum, nsAString& aResult) const
{
  nsSVGElement::StringAttributesInfo info = const_cast<nsSVGElement*>(this)->GetStringInfo();

  NS_ASSERTION(info.mStringCount > 0,
               "GetBaseValue on element with no string attribs");

  NS_ASSERTION(aAttrEnum < info.mStringCount, "aAttrEnum out of range");

  GetAttr(info.mStringInfo[aAttrEnum].mNamespaceID,
          *info.mStringInfo[aAttrEnum].mName, aResult);
}

void nsSVGElement::SetStringBaseValue(PRUint8 aAttrEnum, const nsAString& aValue)
{
  nsSVGElement::StringAttributesInfo info = GetStringInfo();

  NS_ASSERTION(info.mStringCount > 0,
               "SetBaseValue on element with no string attribs");

  NS_ASSERTION(aAttrEnum < info.mStringCount, "aAttrEnum out of range");

  SetAttr(info.mStringInfo[aAttrEnum].mNamespaceID,
          *info.mStringInfo[aAttrEnum].mName, aValue, true);
}

void
nsSVGElement::DidAnimateString(PRUint8 aAttrEnum)
{
  nsIFrame* frame = GetPrimaryFrame();

  if (frame) {
    StringAttributesInfo info = GetStringInfo();
    frame->AttributeChanged(info.mStringInfo[aAttrEnum].mNamespaceID,
                            *info.mStringInfo[aAttrEnum].mName,
                            nsIDOMMutationEvent::MODIFICATION);
  }
}

nsSVGElement::StringListAttributesInfo
nsSVGElement::GetStringListInfo()
{
  return StringListAttributesInfo(nsnull, nsnull, 0);
}

nsAttrValue
nsSVGElement::WillChangeStringList(bool aIsConditionalProcessingAttribute,
                                   PRUint8 aAttrEnum)
{
  nsIAtom* name;
  if (aIsConditionalProcessingAttribute) {
    nsCOMPtr<DOMSVGTests> tests(do_QueryInterface(this));
    name = tests->GetAttrName(aAttrEnum);
  } else {
    name = *GetStringListInfo().mStringListInfo[aAttrEnum].mName;
  }
  return WillChangeValue(name);
}

void
nsSVGElement::DidChangeStringList(bool aIsConditionalProcessingAttribute,
                                  PRUint8 aAttrEnum,
                                  const nsAttrValue& aEmptyOrOldValue)
{
  nsIAtom* name;
  nsAttrValue newValue;
  nsCOMPtr<DOMSVGTests> tests;

  if (aIsConditionalProcessingAttribute) {
    tests = do_QueryInterface(this);
    name = tests->GetAttrName(aAttrEnum);
    tests->GetAttrValue(aAttrEnum, newValue);
  } else {
    StringListAttributesInfo info = GetStringListInfo();

    NS_ASSERTION(info.mStringListCount > 0,
                 "DidChangeStringList on element with no string list attribs");
    NS_ASSERTION(aAttrEnum < info.mStringListCount, "aAttrEnum out of range");

    name = *info.mStringListInfo[aAttrEnum].mName;
    newValue.SetTo(info.mStringLists[aAttrEnum], nsnull);
  }

  DidChangeValue(name, aEmptyOrOldValue, newValue);

  if (aIsConditionalProcessingAttribute) {
    tests->MaybeInvalidate();
  }
}

void
nsSVGElement::StringListAttributesInfo::Reset(PRUint8 aAttrEnum)
{
  mStringLists[aAttrEnum].Clear();
  // caller notifies
}

nsresult
nsSVGElement::ReportAttributeParseFailure(nsIDocument* aDocument,
                                          nsIAtom* aAttribute,
                                          const nsAString& aValue)
{
  const nsAFlatString& attributeValue = PromiseFlatString(aValue);
  const PRUnichar *strings[] = { aAttribute->GetUTF16String(),
                                 attributeValue.get() };
  return nsSVGUtils::ReportToConsole(aDocument,
                                     "AttributeParseWarning",
                                     strings, ArrayLength(strings));
}

void
nsSVGElement::RecompileScriptEventListeners()
{
  PRInt32 i, count = mAttrsAndChildren.AttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName *name = mAttrsAndChildren.AttrNameAt(i);

    // Eventlistenener-attributes are always in the null namespace
    if (!name->IsAtom()) {
        continue;
    }

    nsIAtom *attr = name->Atom();
    if (!IsEventName(attr)) {
      continue;
    }

    nsAutoString value;
    GetAttr(kNameSpaceID_None, attr, value);
    AddScriptEventListener(GetEventNameForAttr(attr), value, true);
  }
}

nsISMILAttr*
nsSVGElement::GetAnimatedAttr(PRInt32 aNamespaceID, nsIAtom* aName)
{
  if (aNamespaceID == kNameSpaceID_None) {
    // Transforms:
    if (GetTransformListAttrName() == aName) {
      SVGAnimatedTransformList* transformList = GetAnimatedTransformList();
      return transformList ?  transformList->ToSMILAttr(this) : nsnull;
    }

    // Motion (fake 'attribute' for animateMotion)
    if (aName == nsGkAtoms::mozAnimateMotionDummyAttr) {
      return new SVGMotionSMILAttr(this);
    }

    // Lengths:
    LengthAttributesInfo info = GetLengthInfo();
    for (PRUint32 i = 0; i < info.mLengthCount; i++) {
      if (aName == *info.mLengthInfo[i].mName) {
        return info.mLengths[i].ToSMILAttr(this);
      }
    }

    // Numbers:
    {
      NumberAttributesInfo info = GetNumberInfo();
      for (PRUint32 i = 0; i < info.mNumberCount; i++) {
        if (aName == *info.mNumberInfo[i].mName) {
          return info.mNumbers[i].ToSMILAttr(this);
        }
      }
    }

    // Number Pairs:
    {
      NumberPairAttributesInfo info = GetNumberPairInfo();
      for (PRUint32 i = 0; i < info.mNumberPairCount; i++) {
        if (aName == *info.mNumberPairInfo[i].mName) {
          return info.mNumberPairs[i].ToSMILAttr(this);
        }
      }
    }

    // Integers:
    {
      IntegerAttributesInfo info = GetIntegerInfo();
      for (PRUint32 i = 0; i < info.mIntegerCount; i++) {
        if (aName == *info.mIntegerInfo[i].mName) {
          return info.mIntegers[i].ToSMILAttr(this);
        }
      }
    }

    // Integer Pairs:
    {
      IntegerPairAttributesInfo info = GetIntegerPairInfo();
      for (PRUint32 i = 0; i < info.mIntegerPairCount; i++) {
        if (aName == *info.mIntegerPairInfo[i].mName) {
          return info.mIntegerPairs[i].ToSMILAttr(this);
        }
      }
    }

    // Enumerations:
    {
      EnumAttributesInfo info = GetEnumInfo();
      for (PRUint32 i = 0; i < info.mEnumCount; i++) {
        if (aName == *info.mEnumInfo[i].mName) {
          return info.mEnums[i].ToSMILAttr(this);
        }
      }
    }

    // Booleans:
    {
      BooleanAttributesInfo info = GetBooleanInfo();
      for (PRUint32 i = 0; i < info.mBooleanCount; i++) {
        if (aName == *info.mBooleanInfo[i].mName) {
          return info.mBooleans[i].ToSMILAttr(this);
        }
      }
    }

    // Angles:
    {
      AngleAttributesInfo info = GetAngleInfo();
      for (PRUint32 i = 0; i < info.mAngleCount; i++) {
        if (aName == *info.mAngleInfo[i].mName) {
          return info.mAngles[i].ToSMILAttr(this);
        }
      }
    }

    // viewBox:
    if (aName == nsGkAtoms::viewBox) {
      nsSVGViewBox *viewBox = GetViewBox();
      return viewBox ? viewBox->ToSMILAttr(this) : nsnull;
    }

    // preserveAspectRatio:
    if (aName == nsGkAtoms::preserveAspectRatio) {
      SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
        GetPreserveAspectRatio();
      return preserveAspectRatio ?
        preserveAspectRatio->ToSMILAttr(this) : nsnull;
    }

    // NumberLists:
    {
      NumberListAttributesInfo info = GetNumberListInfo();
      for (PRUint32 i = 0; i < info.mNumberListCount; i++) {
        if (aName == *info.mNumberListInfo[i].mName) {
          NS_ABORT_IF_FALSE(i <= UCHAR_MAX, "Too many attributes");
          return info.mNumberLists[i].ToSMILAttr(this, PRUint8(i));
        }
      }
    }

    // LengthLists:
    {
      LengthListAttributesInfo info = GetLengthListInfo();
      for (PRUint32 i = 0; i < info.mLengthListCount; i++) {
        if (aName == *info.mLengthListInfo[i].mName) {
          NS_ABORT_IF_FALSE(i <= UCHAR_MAX, "Too many attributes");
          return info.mLengthLists[i].ToSMILAttr(this,
                                                 PRUint8(i),
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

    // Mapped attributes:
    if (IsAttributeMapped(aName)) {
      nsCSSProperty prop =
        nsCSSProps::LookupProperty(nsDependentAtomString(aName));
      // Check IsPropertyAnimatable to avoid attributes that...
      //  - map to explicitly unanimatable properties (e.g. 'direction')
      //  - map to unsupported attributes (e.g. 'glyph-orientation-horizontal')
      if (nsSMILCSSProperty::IsPropertyAnimatable(prop)) {
        return new nsSMILMappedAttribute(prop, this);
      }
    }
  }

  // Strings
  {
    StringAttributesInfo info = GetStringInfo();
    for (PRUint32 i = 0; i < info.mStringCount; i++) {
      if (aNamespaceID == info.mStringInfo[i].mNamespaceID &&
          aName == *info.mStringInfo[i].mName) {
        return info.mStrings[i].ToSMILAttr(this);
      }
    }
  }

  return nsnull;
}

void
nsSVGElement::AnimationNeedsResample()
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc && doc->HasAnimationController()) {
    doc->GetAnimationController()->SetResampleNeeded();
  }
}

void
nsSVGElement::FlushAnimations()
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc && doc->HasAnimationController()) {
    doc->GetAnimationController()->FlushResampleRequests();
  }
}
