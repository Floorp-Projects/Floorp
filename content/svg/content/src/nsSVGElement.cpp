/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "nsSVGElement.h"
#include "nsSVGSVGElement.h"
#include "nsSVGSwitchElement.h"
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
#include "nsIXBLService.h"
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
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGRect.h"
#include "nsIFrame.h"
#include "prdtoa.h"
#include <stdarg.h>
#include "nsSMILMappedAttribute.h"
#include "SVGMotionSMILAttr.h"

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
                           const nsAString* aValue, bool aNotify)
{  
  // If this is an svg presentation attribute we need to map it into
  // the content stylerule.
  // XXX For some reason incremental mapping doesn't work, so for now
  // just delete the style rule and lazily reconstruct it in
  // GetContentStyleRule()
  if (aNamespaceID == kNameSpaceID_None && IsAttributeMapped(aName)) {
    mContentStyleRule = nsnull;
  }

  if (IsEventName(aName) && aValue) {
    nsresult rv = AddScriptEventListener(GetEventNameForAttr(aName), *aValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aNamespaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::requiredFeatures ||
       aName == nsGkAtoms::requiredExtensions ||
       aName == nsGkAtoms::systemLanguage)) {

    nsIContent* parent = GetFlattenedTreeParent();
  
    if (parent &&
        parent->NodeInfo()->Equals(nsGkAtoms::svgSwitch, kNameSpaceID_SVG)) {
      static_cast<nsSVGSwitchElement*>(parent)->MaybeInvalidate();
    }
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
  if (aNamespaceID == kNameSpaceID_None) {

    // Check for nsSVGLength2 attribute
    LengthAttributesInfo lengthInfo = GetLengthInfo();

    PRUint32 i;
    for (i = 0; i < lengthInfo.mLengthCount; i++) {
      if (aAttribute == *lengthInfo.mLengthInfo[i].mName) {
        rv = lengthInfo.mLengths[i].SetBaseValueString(aValue, this, false);
        if (NS_FAILED(rv)) {
          lengthInfo.Reset(i);
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
          rv = pointList->SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            // The spec says we parse everything up to the failure, so we don't
            // call pointList->ClearBaseValue()
          }
          foundMatch = true;
        }
      }
    }

    if (!foundMatch) {
      // Check for SVGAnimatedPathSegList attribute
      if (GetPathDataAttrName() == aAttribute) {
        SVGAnimatedPathSegList* segList = GetAnimPathSegList();
        if (segList) {
          rv = segList->SetBaseValueString(aValue);
          if (NS_FAILED(rv)) {
            // The spec says we parse everything up to the failure, so we don't
            // call segList->ClearBaseValue()
          }
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
          rv = integerPairInfo.mIntegerPairs[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            integerPairInfo.Reset(i);
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
          rv = booleanInfo.mBooleans[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            booleanInfo.Reset(i);
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
          rv = enumInfo.mEnums[i].SetBaseValueString(aValue, this);
          if (NS_FAILED(rv)) {
            enumInfo.Reset(i);
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
    aResult.SetTo(aValue);
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
        lenInfo.Reset(i);
        DidChangeLength(i, false);
        return;
      }
    }

    // Check if this is a length list attribute going away
    LengthListAttributesInfo lengthListInfo = GetLengthListInfo();

    for (PRUint32 i = 0; i < lengthListInfo.mLengthListCount; i++) {
      if (aName == *lengthListInfo.mLengthListInfo[i].mName) {
        lengthListInfo.Reset(i);
        DidChangeLengthList(i, false);
        return;
      }
    }

    // Check if this is a number list attribute going away
    NumberListAttributesInfo numberListInfo = GetNumberListInfo();

    for (PRUint32 i = 0; i < numberListInfo.mNumberListCount; i++) {
      if (aName == *numberListInfo.mNumberListInfo[i].mName) {
        numberListInfo.Reset(i);
        DidChangeNumberList(i, false);
        return;
      }
    }

    // Check if this is a point list attribute going away
    if (GetPointListAttrName() == aName) {
      SVGAnimatedPointList *pointList = GetAnimatedPointList();
      if (pointList) {
        pointList->ClearBaseValue();
        return;
      }
    }

    // Check if this is a path segment list attribute going away
    if (GetPathDataAttrName() == aName) {
      SVGAnimatedPathSegList *segList = GetAnimPathSegList();
      if (segList) {
        segList->ClearBaseValue();
        DidChangePathSegList(false);
        return;
      }
    }

    // Check if this is a number attribute going away
    NumberAttributesInfo numInfo = GetNumberInfo();

    for (PRUint32 i = 0; i < numInfo.mNumberCount; i++) {
      if (aName == *numInfo.mNumberInfo[i].mName) {
        numInfo.Reset(i);
        DidChangeNumber(i, false);
        return;
      }
    }

    // Check if this is a number pair attribute going away
    NumberPairAttributesInfo numPairInfo = GetNumberPairInfo();

    for (PRUint32 i = 0; i < numPairInfo.mNumberPairCount; i++) {
      if (aName == *numPairInfo.mNumberPairInfo[i].mName) {
        numPairInfo.Reset(i);
        DidChangeNumberPair(i, false);
        return;
      }
    }

    // Check if this is an integer attribute going away
    IntegerAttributesInfo intInfo = GetIntegerInfo();

    for (PRUint32 i = 0; i < intInfo.mIntegerCount; i++) {
      if (aName == *intInfo.mIntegerInfo[i].mName) {
        intInfo.Reset(i);
        DidChangeInteger(i, false);
        return;
      }
    }

    // Check if this is an integer pair attribute going away
    IntegerPairAttributesInfo intPairInfo = GetIntegerPairInfo();

    for (PRUint32 i = 0; i < intPairInfo.mIntegerPairCount; i++) {
      if (aName == *intPairInfo.mIntegerPairInfo[i].mName) {
        intPairInfo.Reset(i);
        DidChangeIntegerPair(i, false);
        return;
      }
    }

    // Check if this is an angle attribute going away
    AngleAttributesInfo angleInfo = GetAngleInfo();

    for (PRUint32 i = 0; i < angleInfo.mAngleCount; i++) {
      if (aName == *angleInfo.mAngleInfo[i].mName) {
        angleInfo.Reset(i);
        DidChangeAngle(i, false);
        return;
      }
    }

    // Check if this is a boolean attribute going away
    BooleanAttributesInfo boolInfo = GetBooleanInfo();

    for (PRUint32 i = 0; i < boolInfo.mBooleanCount; i++) {
      if (aName == *boolInfo.mBooleanInfo[i].mName) {
        boolInfo.Reset(i);
        DidChangeBoolean(i, false);
        return;
      }
    }

    // Check if this is an enum attribute going away
    EnumAttributesInfo enumInfo = GetEnumInfo();

    for (PRUint32 i = 0; i < enumInfo.mEnumCount; i++) {
      if (aName == *enumInfo.mEnumInfo[i].mName) {
        enumInfo.Reset(i);
        DidChangeEnum(i, false);
        return;
      }
    }

    // Check if this is a nsViewBox attribute going away
    if (aName == nsGkAtoms::viewBox) {
      nsSVGViewBox* viewBox = GetViewBox();
      if (viewBox) {
        viewBox->Init();
        DidChangeViewBox(false);
        return;
      }
    }

    // Check if this is a preserveAspectRatio attribute going away
    if (aName == nsGkAtoms::preserveAspectRatio) {
      SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
        GetPreserveAspectRatio();

      if (preserveAspectRatio) {
        preserveAspectRatio->Init();
        DidChangePreserveAspectRatio(false);
        return;
      }
    }

    // Check if this is a transform list attribute going away
    if (GetTransformListAttrName() == aName) {
      SVGAnimatedTransformList *transformList = GetAnimatedTransformList();
      if (transformList) {
        transformList->ClearBaseValue();
        DidChangeTransformList(false);
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
      DidChangeString(i);
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

  if (aAttribute == nsGkAtoms::requiredFeatures ||
      aAttribute == nsGkAtoms::requiredExtensions ||
      aAttribute == nsGkAtoms::systemLanguage) {
    // It would be nice to only reconstruct the frame if the value returned by
    // NS_SVG_PassesConditionalProcessingTests has changed, but we don't know
    // that
    NS_UpdateHint(retval, nsChangeHint_ReconstructFrame);
  }
  return retval;
}

bool
nsSVGElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eSVG));
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
  bool changed; // outparam for ParseProperty. (ignored)
  mParser.ParseProperty(propertyID, aMappedAttrValue, mDocURI, mBaseURI,
                        mNodePrincipal, mDecl, &changed, false);
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

  while (ancestor && ancestor->GetNameSpaceID() == kNameSpaceID_SVG) {
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
nsSVGElement::PrependLocalTransformTo(const gfxMatrix &aMatrix) const
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

void
nsSVGElement::DidChangeLength(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mLengthCount > 0,
               "DidChangeLength on element with no length attribs");

  NS_ASSERTION(aAttrEnum < info.mLengthCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mLengths[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, *info.mLengthInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
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

void
nsSVGElement::DidChangeLengthList(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  LengthListAttributesInfo info = GetLengthListInfo();

  NS_ASSERTION(info.mLengthListCount > 0,
               "DidChangeLengthList on element with no length list attribs");
  NS_ASSERTION(aAttrEnum < info.mLengthListCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mLengthLists[aAttrEnum].GetBaseValue().GetValueAsString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, *info.mLengthListInfo[aAttrEnum].mName,
                nsnull, attrValue, true);
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

void
nsSVGElement::DidChangeNumberList(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  NumberListAttributesInfo info = GetNumberListInfo();

  NS_ABORT_IF_FALSE(info.mNumberListCount > 0,
                    "DidChangeNumberList on element with no number list attribs");
  NS_ABORT_IF_FALSE(aAttrEnum < info.mNumberListCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mNumberLists[aAttrEnum].GetBaseValue().GetValueAsString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, *info.mNumberListInfo[aAttrEnum].mName,
                nsnull, attrValue, true);
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

void
nsSVGElement::DidChangePointList(bool aDoSetAttr)
{
  NS_ABORT_IF_FALSE(GetPointListAttrName(), "Changing non-existent point list?");

  if (!aDoSetAttr)
    return;

  nsAutoString serializedValue;
  GetAnimatedPointList()->GetBaseValue().GetValueAsString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, GetPointListAttrName(), nsnull,
                attrValue, true);
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

void
nsSVGElement::DidChangePathSegList(bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  nsAutoString serializedValue;
  GetAnimPathSegList()->GetBaseValue().GetValueAsString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, GetPathDataAttrName(), nsnull,
                attrValue, true);
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
nsSVGElement::DidChangeNumber(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mNumberCount > 0,
               "DidChangeNumber on element with no number attribs");

  NS_ASSERTION(aAttrEnum < info.mNumberCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mNumbers[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
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

void
nsSVGElement::DidChangeNumberPair(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  NumberPairAttributesInfo info = GetNumberPairInfo();

  NS_ASSERTION(info.mNumberPairCount > 0,
               "DidChangePairNumber on element with no number pair attribs");

  NS_ASSERTION(aAttrEnum < info.mNumberPairCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mNumberPairs[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, *info.mNumberPairInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
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
nsSVGElement::DidChangeInteger(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mIntegerCount > 0,
               "DidChangeInteger on element with no integer attribs");

  NS_ASSERTION(aAttrEnum < info.mIntegerCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mIntegers[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
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

void
nsSVGElement::DidChangeIntegerPair(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  IntegerPairAttributesInfo info = GetIntegerPairInfo();

  NS_ASSERTION(info.mIntegerPairCount > 0,
               "DidChangeIntegerPair on element with no integer pair attribs");

  NS_ASSERTION(aAttrEnum < info.mIntegerPairCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mIntegerPairs[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, *info.mIntegerPairInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
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

void
nsSVGElement::DidChangeAngle(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  AngleAttributesInfo info = GetAngleInfo();

  NS_ASSERTION(info.mAngleCount > 0,
               "DidChangeAngle on element with no angle attribs");

  NS_ASSERTION(aAttrEnum < info.mAngleCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mAngles[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, *info.mAngleInfo[aAttrEnum].mName, nsnull,
                attrValue, true);
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
nsSVGElement::DidChangeBoolean(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  BooleanAttributesInfo info = GetBooleanInfo();

  NS_ASSERTION(info.mBooleanCount > 0,
               "DidChangeBoolean on element with no boolean attribs");

  NS_ASSERTION(aAttrEnum < info.mBooleanCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mBooleans[aAttrEnum].GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
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
nsSVGElement::DidChangeEnum(PRUint8 aAttrEnum, bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  EnumAttributesInfo info = GetEnumInfo();

  NS_ASSERTION(info.mEnumCount > 0,
               "DidChangeEnum on element with no enum attribs");

  NS_ASSERTION(aAttrEnum < info.mEnumCount, "aAttrEnum out of range");

  nsAutoString serializedValue;
  info.mEnums[aAttrEnum].GetBaseValueString(serializedValue, this);

  nsAttrValue attrValue(serializedValue);
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

void
nsSVGElement::DidChangeViewBox(bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  nsSVGViewBox *viewBox = GetViewBox();

  NS_ASSERTION(viewBox, "DidChangeViewBox on element with no viewBox attrib");

  nsAutoString serializedValue;
  viewBox->GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, nsGkAtoms::viewBox, nsnull,
                attrValue, true);
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

void
nsSVGElement::DidChangePreserveAspectRatio(bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  SVGAnimatedPreserveAspectRatio *preserveAspectRatio =
    GetPreserveAspectRatio();

  NS_ASSERTION(preserveAspectRatio,
               "DidChangePreserveAspectRatio on element with no preserveAspectRatio attrib");

  nsAutoString serializedValue;
  preserveAspectRatio->GetBaseValueString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, nsGkAtoms::preserveAspectRatio, nsnull,
                attrValue, true);
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

void
nsSVGElement::DidChangeTransformList(bool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  SVGAnimatedTransformList* transformList = GetAnimatedTransformList();
  NS_ABORT_IF_FALSE(transformList,
                    "DidChangeTransformList on element with no transform list");

  nsAutoString serializedValue;
  transformList->GetBaseValue().GetValueAsString(serializedValue);

  nsAttrValue attrValue(serializedValue);
  SetParsedAttr(kNameSpaceID_None, GetTransformListAttrName(), nsnull,
                attrValue, true);
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
