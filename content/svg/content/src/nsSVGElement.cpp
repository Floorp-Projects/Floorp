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
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIServiceManager.h"
#include "nsIXBLService.h"
#include "nsGkAtoms.h"
#include "nsICSSStyleRule.h"
#include "nsRuleWalker.h"
#include "nsCSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsGenericHTMLElement.h"
#include "nsNodeInfoManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIEventListenerManager.h"
#include "nsSVGUtils.h"
#include "nsSVGLength2.h"
#include "nsSVGNumber2.h"
#include "nsSVGInteger.h"
#include "nsSVGAngle.h"
#include "nsSVGBoolean.h"
#include "nsSVGEnum.h"
#include "nsSVGString.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsIDOMSVGNumberList.h"
#include "nsIDOMSVGAnimatedNumberList.h"
#include "nsIDOMSVGPointList.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsIDOMSVGPresAspectRatio.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsSVGRect.h"
#include "nsIFrame.h"
#include "prdtoa.h"
#include <stdarg.h>
#ifdef MOZ_SMIL
#include "nsIDOMSVGTransformable.h"
#endif // MOZ_SMIL

nsSVGEnumMapping nsSVGElement::sSVGUnitTypesMap[] = {
  {&nsGkAtoms::userSpaceOnUse, nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE},
  {&nsGkAtoms::objectBoundingBox, nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
  {nsnull, 0}
};

nsSVGElement::nsSVGElement(nsINodeInfo *aNodeInfo)
  : nsSVGElementBase(aNodeInfo), mSuppressNotification(PR_FALSE)
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

  IntegerAttributesInfo integerInfo = GetIntegerInfo();

  for (i = 0; i < integerInfo.mIntegerCount; i++) {
    integerInfo.Reset(i);
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

  nsSVGPreserveAspectRatio *preserveAspectRatio =
    GetPreserveAspectRatio();

  if (preserveAspectRatio) {
    preserveAspectRatio->Init();
  }

  StringAttributesInfo stringInfo = GetStringInfo();

  for (i = 0; i < stringInfo.mStringCount; i++) {
    stringInfo.Reset(i);
  }

  return NS_OK;
}

nsSVGElement::~nsSVGElement()
{
  PRUint32 i, count = mMappedAttributes.AttrCount();
  for (i = 0; i < count; ++i) {
    mMappedAttributes.AttrAt(i)->GetSVGValue()->RemoveObserver(this);
  }
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGElement, nsSVGElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGElement, nsSVGElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
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
                         PRBool aCompileEventHandlers)
{
  nsresult rv = nsSVGElementBase::BindToTree(aDocument, aParent,
                                             aBindingParent,
                                             aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!HasFlag(NODE_MAY_HAVE_STYLE)) {
    return NS_OK;
  }
  const nsAttrValue* oldVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);

  if (oldVal && oldVal->Type() == nsAttrValue::eCSSStyleRule) {
    // we need to force a reparse because the baseURI of the document
    // may have changed
    nsAttrValue attrValue;
    nsAutoString stringValue;
    oldVal->ToString(stringValue);
    // Force in data doc, since we already have a style rule
    ParseStyleAttribute(this, stringValue, attrValue, PR_TRUE);
    // Don't bother going through SetInlineStyleRule, we don't want to fire off
    // mutation events or document notifications anyway
    rv = mAttrsAndChildren.SetAndTakeAttr(nsGkAtoms::style, attrValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsSVGElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                            const nsAString* aValue, PRBool aNotify)
{
  // If this is an svg presentation attribute we need to map it into
  // the content stylerule.
  // XXX For some reason incremental mapping doesn't work, so for now
  // just delete the style rule and lazily reconstruct it in
  // GetContentStyleRule()
  if (aNamespaceID == kNameSpaceID_None && IsAttributeMapped(aName)) {
    mContentStyleRule = nsnull;
  }

  return nsSVGElementBase::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
}

nsresult
nsSVGElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                           const nsAString* aValue, PRBool aNotify)
{  
  if (IsEventName(aName) && aValue) {
    nsresult rv = AddScriptEventListener(GetEventNameForAttr(aName), *aValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aNamespaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::requiredFeatures ||
       aName == nsGkAtoms::requiredExtensions ||
       aName == nsGkAtoms::systemLanguage)) {

    nsIContent* parent = nsnull;
  
    nsIContent* bindingParent = GetBindingParent();
    if (bindingParent) {
      nsIDocument* doc = bindingParent->GetOwnerDoc();
      if (doc) {
        parent = doc->BindingManager()->GetInsertionParent(bindingParent);
      }
    }

    if (!parent) {
      // if we didn't find an anonymous parent, use the explicit one,
      // whether it's null or not...
      parent = GetParent();
    }

    if (parent &&
        parent->NodeInfo()->Equals(nsGkAtoms::svgSwitch, kNameSpaceID_SVG)) {
      static_cast<nsSVGSwitchElement*>(parent)->MaybeInvalidate();
    }
  }

  return nsSVGElementBase::AfterSetAttr(aNamespaceID, aName, aValue, aNotify);
}

PRBool
nsSVGElement::ParseAttribute(PRInt32 aNamespaceID,
                             nsIAtom* aAttribute,
                             const nsAString& aValue,
                             nsAttrValue& aResult)
{
  // Parse value
  nsCOMPtr<nsISVGValue> svg_value;
  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aAttribute, aNamespaceID);
  if (val) {
    // Found the attr in the list.
    if (val->Type() == nsAttrValue::eSVGValue) {
      svg_value = val->GetSVGValue();
    }
  }
  else {
    // Could be a mapped attribute.
    svg_value = GetMappedAttribute(aNamespaceID, aAttribute);
  }
  
  if (svg_value) {
    // We want to prevent DidModifySVGObservable from running if we
    // come in this route, otherwise AttributeChanged() gets called
    // twice (once through DidMOdifySVGObservable, once through SetAttr).
    mSuppressNotification = PR_TRUE;

    if (NS_FAILED(svg_value->SetValueString(aValue))) {
      // The value was rejected. This happens e.g. in a XUL template
      // when trying to set a value like "?x" on a value object that
      // expects a length.
      // To accommodate this "erroneous" value, we'll insert a proxy
      // object between ourselves and the actual value object:
      ReportAttributeParseFailure(GetOwnerDoc(), aAttribute, aValue);
      nsCOMPtr<nsISVGValue> proxy;
      nsresult rv =
        NS_CreateSVGStringProxyValue(svg_value, getter_AddRefs(proxy));
      // Failure means we'll store this attr as a string, not an SVGValue, but
      // that's the best we can do short of throwing outright.
      NS_ENSURE_SUCCESS(rv, PR_FALSE);

      svg_value->RemoveObserver(this);
      ResetOldStyleBaseType(svg_value);
      proxy->SetValueString(aValue);
      proxy->AddObserver(this);
      aResult.SetTo(proxy);
    }
    else {
      aResult.SetTo(svg_value);
    }
    mSuppressNotification = PR_FALSE;
    return PR_TRUE;
  }

  nsresult rv = NS_OK;
  PRBool foundMatch = PR_FALSE;
  if (aNamespaceID == kNameSpaceID_None) {

    // Check for nsSVGLength2 attribute
    LengthAttributesInfo lengthInfo = GetLengthInfo();

    PRUint32 i;
    for (i = 0; i < lengthInfo.mLengthCount; i++) {
      if (aAttribute == *lengthInfo.mLengthInfo[i].mName) {
        rv = lengthInfo.mLengths[i].SetBaseValueString(aValue, this, PR_FALSE);
        if (NS_FAILED(rv)) {
          lengthInfo.Reset(i);
        }
        foundMatch = PR_TRUE;
        break;
      }
    }

    if (!foundMatch) {
      // Check for nsSVGNumber2 attribute
      NumberAttributesInfo numberInfo = GetNumberInfo();
      for (i = 0; i < numberInfo.mNumberCount; i++) {
        if (aAttribute == *numberInfo.mNumberInfo[i].mName) {
          if (i + 1 < numberInfo.mNumberCount &&
              aAttribute == *numberInfo.mNumberInfo[i + 1].mName) {
            rv = ParseNumberOptionalNumber(aValue, i, i + 1);
            if (NS_FAILED(rv)) {
              numberInfo.Reset(i + 1);
            }
          } else {
            rv = numberInfo.mNumbers[i].SetBaseValueString(aValue, this, PR_FALSE);
          }
          if (NS_FAILED(rv)) {
            numberInfo.Reset(i);
          }
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGInteger attribute
      IntegerAttributesInfo integerInfo = GetIntegerInfo();
      for (i = 0; i < integerInfo.mIntegerCount; i++) {
        if (aAttribute == *integerInfo.mIntegerInfo[i].mName) {
          if (i + 1 < integerInfo.mIntegerCount &&
              aAttribute == *integerInfo.mIntegerInfo[i + 1].mName) {
            rv = ParseIntegerOptionalInteger(aValue, i, i + 1);
            if (NS_FAILED(rv)) {
              integerInfo.Reset(i + 1);
            }
          } else {
            rv = integerInfo.mIntegers[i].SetBaseValueString(aValue, this, PR_FALSE);
          }
          if (NS_FAILED(rv)) {
            integerInfo.Reset(i);
          }
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGAngle attribute
      AngleAttributesInfo angleInfo = GetAngleInfo();
      for (i = 0; i < angleInfo.mAngleCount; i++) {
        if (aAttribute == *angleInfo.mAngleInfo[i].mName) {
          rv = angleInfo.mAngles[i].SetBaseValueString(aValue, this, PR_FALSE);
          if (NS_FAILED(rv)) {
            angleInfo.Reset(i);
          }
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGBoolean attribute
      BooleanAttributesInfo booleanInfo = GetBooleanInfo();
      for (i = 0; i < booleanInfo.mBooleanCount; i++) {
        if (aAttribute == *booleanInfo.mBooleanInfo[i].mName) {
          rv = booleanInfo.mBooleans[i].SetBaseValueString(aValue, this, PR_FALSE);
          if (NS_FAILED(rv)) {
            booleanInfo.Reset(i);
          }
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check for nsSVGEnum attribute
      EnumAttributesInfo enumInfo = GetEnumInfo();
      for (i = 0; i < enumInfo.mEnumCount; i++) {
        if (aAttribute == *enumInfo.mEnumInfo[i].mName) {
          rv = enumInfo.mEnums[i].SetBaseValueString(aValue, this, PR_FALSE);
          if (NS_FAILED(rv)) {
            enumInfo.Reset(i);
          }
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch && aAttribute == nsGkAtoms::preserveAspectRatio) {
      // Check for nsSVGPreserveAspectRatio attribute
      nsSVGPreserveAspectRatio *preserveAspectRatio =
        GetPreserveAspectRatio();
      if (preserveAspectRatio) {
        rv = preserveAspectRatio->SetBaseValueString(aValue, this, PR_FALSE);
        if (NS_FAILED(rv)) {
          preserveAspectRatio->Init();
        }
        foundMatch = PR_TRUE;
      }
    }
  }

  if (!foundMatch) {
    // Check for nsSVGString attribute
    StringAttributesInfo stringInfo = GetStringInfo();
    for (PRUint32 i = 0; i < stringInfo.mStringCount; i++) {
      if (aNamespaceID == stringInfo.mStringInfo[i].mNamespaceID &&
          aAttribute == *stringInfo.mStringInfo[i].mName) {
        stringInfo.mStrings[i].SetBaseValue(aValue, this, PR_FALSE);
        foundMatch = PR_TRUE;
        break;
      }
    }
  }

  if (foundMatch) {
    if (NS_FAILED(rv)) {
      ReportAttributeParseFailure(GetOwnerDoc(), aAttribute, aValue);
      return PR_FALSE;
    }
    aResult.SetTo(aValue);
    return PR_TRUE;
  }

  return nsSVGElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                          aResult);
}

nsresult
nsSVGElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                        PRBool aNotify)
{
  PRBool foundMatch = PR_FALSE;

  if (aNamespaceID == kNameSpaceID_None) {
    // If this is an svg presentation attribute, remove rule to force an update
    if (IsAttributeMapped(aName))
      mContentStyleRule = nsnull;

    if (IsEventName(aName)) {
      nsCOMPtr<nsIEventListenerManager> manager;
      GetListenerManager(PR_FALSE, getter_AddRefs(manager));
      if (manager) {
        nsIAtom* eventName = GetEventNameForAttr(aName);
        manager->RemoveScriptEventListener(eventName);
      }
      foundMatch = PR_TRUE;
    }
    
    if (!foundMatch) {
      // Check if this is a length attribute going away
      LengthAttributesInfo lenInfo = GetLengthInfo();

      for (PRUint32 i = 0; i < lenInfo.mLengthCount; i++) {
        if (aName == *lenInfo.mLengthInfo[i].mName) {
          lenInfo.Reset(i);
          DidChangeLength(i, PR_FALSE);
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check if this is a number attribute going away
      NumberAttributesInfo numInfo = GetNumberInfo();

      for (PRUint32 i = 0; i < numInfo.mNumberCount; i++) {
        if (aName == *numInfo.mNumberInfo[i].mName) {
          if (i + 1 < numInfo.mNumberCount &&
              aName == *numInfo.mNumberInfo[i + 1].mName) {
            // found a number-optional-number
            numInfo.Reset(i + 1);
            DidChangeNumber(i + 1, PR_FALSE);
          }
          numInfo.Reset(i);
          DidChangeNumber(i, PR_FALSE);
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check if this is an integer attribute going away
      IntegerAttributesInfo intInfo = GetIntegerInfo();

      for (PRUint32 i = 0; i < intInfo.mIntegerCount; i++) {
        if (aName == *intInfo.mIntegerInfo[i].mName) {
          if (i + 1 < intInfo.mIntegerCount &&
              aName == *intInfo.mIntegerInfo[i + 1].mName) {
            // found a number-optional-number
            intInfo.Reset(i + 1);
            DidChangeNumber(i + 1, PR_FALSE);
          }
          intInfo.Reset(i);
          DidChangeInteger(i, PR_FALSE);
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check if this is an angle attribute going away
      AngleAttributesInfo angleInfo = GetAngleInfo();

      for (PRUint32 i = 0; i < angleInfo.mAngleCount; i++) {
        if (aName == *angleInfo.mAngleInfo[i].mName) {
          angleInfo.Reset(i);
          DidChangeAngle(i, PR_FALSE);
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch) {
      // Check if this is a boolean attribute going away
      BooleanAttributesInfo boolInfo = GetBooleanInfo();

      for (PRUint32 i = 0; i < boolInfo.mBooleanCount; i++) {
        if (aName == *boolInfo.mBooleanInfo[i].mName) {
          boolInfo.Reset(i);
          DidChangeBoolean(i, PR_FALSE);
          foundMatch = PR_TRUE;
        }
      }
    }

    if (!foundMatch) {
      // Check if this is an enum attribute going away
      EnumAttributesInfo enumInfo = GetEnumInfo();

      for (PRUint32 i = 0; i < enumInfo.mEnumCount; i++) {
        if (aName == *enumInfo.mEnumInfo[i].mName) {
          enumInfo.Reset(i);
          DidChangeEnum(i, PR_FALSE);
          foundMatch = PR_TRUE;
          break;
        }
      }
    }

    if (!foundMatch && aName == nsGkAtoms::preserveAspectRatio) {
      // Check if this is a preserveAspectRatio attribute going away
      nsSVGPreserveAspectRatio *preserveAspectRatio =
        GetPreserveAspectRatio();

      if (preserveAspectRatio) {
        preserveAspectRatio->Init();
        DidChangePreserveAspectRatio(PR_FALSE);
        foundMatch = PR_TRUE;
      }
    }
  }

  if (!foundMatch) {
    // Check if this is a string attribute going away
    StringAttributesInfo stringInfo = GetStringInfo();

    for (PRUint32 i = 0; i < stringInfo.mStringCount; i++) {
      if (aNamespaceID == stringInfo.mStringInfo[i].mNamespaceID &&
          aName == *stringInfo.mStringInfo[i].mName) {
        stringInfo.Reset(i);
        DidChangeString(i);
        foundMatch = PR_TRUE;
        break;
      }
    }
  }

  if (!foundMatch) {
    // Now check for one of the old style basetypes going away
    nsCOMPtr<nsISVGValue> svg_value = GetMappedAttribute(aNamespaceID, aName);

    if (svg_value) {
      mSuppressNotification = PR_TRUE;
      ResetOldStyleBaseType(svg_value);
      mSuppressNotification = PR_FALSE;
    }
  }

  return nsSVGElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

void
nsSVGElement::ResetOldStyleBaseType(nsISVGValue *svg_value)
{
  nsCOMPtr<nsIDOMSVGAnimatedRect> r = do_QueryInterface(svg_value);
  if (r) {
    nsCOMPtr<nsIDOMSVGRect> rect;
    r->GetBaseVal(getter_AddRefs(rect));
    static_cast<nsSVGRect*>(rect.get())->Clear();
  }
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> ll = do_QueryInterface(svg_value);
  if (ll) {
    nsCOMPtr<nsIDOMSVGLengthList> lengthlist;
    ll->GetBaseVal(getter_AddRefs(lengthlist));
    lengthlist->Clear();
  }
  nsCOMPtr<nsIDOMSVGAnimatedNumberList> nl = do_QueryInterface(svg_value);
  if (nl) {
    nsCOMPtr<nsIDOMSVGNumberList> numberlist;
    nl->GetBaseVal(getter_AddRefs(numberlist));
    numberlist->Clear();
  }
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> tl = do_QueryInterface(svg_value);
  if (tl) {
    nsCOMPtr<nsIDOMSVGTransformList> transform;
    tl->GetBaseVal(getter_AddRefs(transform));
    transform->Clear();
  }
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

PRBool
nsSVGElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eELEMENT | eSVG));
}

already_AddRefed<nsIURI>
nsSVGElement::GetBaseURI() const
{
  nsCOMPtr<nsIURI> baseURI = nsSVGElementBase::GetBaseURI();

  nsIContent* bindingParent = GetBindingParent();
  if (bindingParent) {
    nsIDocument* doc = bindingParent->GetOwnerDoc();
    if (doc) {
      nsXBLBinding* binding = doc->BindingManager()->GetBinding(bindingParent);
      if (binding) {
        // XXX sXBL/XBL2 issue
        // If this is an anonymous XBL element use the binding
        // document for the base URI. 
        // XXX Will fail with xml:base
        baseURI = binding->PrototypeBinding()->DocURI();
      }
    }
  }
  return baseURI.forget();
}

NS_IMETHODIMP
nsSVGElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
#ifdef DEBUG
//  printf("nsSVGElement(%p)::WalkContentStyleRules()\n", this);
#endif
  if (!mContentStyleRule)
    UpdateContentStyleRule();

  if (mContentStyleRule)  
    aRuleWalker->Forward(mContentStyleRule);

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
nsSVGElement::IsSupported(const nsAString& aFeature, const nsAString& aVersion, PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("nsSVGElement::IsSupported");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  return SetAttr(kNameSpaceID_None, nsGkAtoms::id, aId, PR_TRUE);
}

/* readonly attribute nsIDOMSVGSVGElement ownerSVGElement; */
NS_IMETHODIMP
nsSVGElement::GetOwnerSVGElement(nsIDOMSVGSVGElement * *aOwnerSVGElement)
{
  *aOwnerSVGElement = nsnull;

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the binding that gives us our anonymous parent.
  // That's the binding manager for the document we actually belong to, which
  // is our owner doc.
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsIContent* parent = nsnull;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    parent = bindingManager->GetInsertionParent(this);
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }

  while (parent && parent->GetNameSpaceID() == kNameSpaceID_SVG) {
    nsIAtom* tag = parent->Tag();
    if (tag == nsGkAtoms::foreignObject) {
      // SVG in a foreignObject must have its own <svg> (nsSVGOuterSVGFrame).
      // Leave *aOwnerSVGElement nulled out, but don't throw.
      return NS_OK;
    }
    if (tag == nsGkAtoms::svg) {
      *aOwnerSVGElement = static_cast<nsSVGSVGElement*>(parent);
      NS_ADDREF(*aOwnerSVGElement);
      return NS_OK;
    }
    nsIContent* next = nsnull;

    if (bindingManager) {
      next = bindingManager->GetInsertionParent(parent);
    }
    if (!next) {
      // no anonymous parent, so use explicit one
      next = parent->GetParent();
    }
    
    parent = next;
  }

  // we don't have a parent SVG element...

  // are _we_ the outermost SVG element? If yes, return nsnull, but don't fail
  if (Tag() == nsGkAtoms::svg) {
    return NS_OK;
  }
  
  // no owner found and we aren't the outermost SVG element either.
  // this situation can e.g. occur during content tree teardown. 
  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIDOMSVGElement viewportElement; */
NS_IMETHODIMP
nsSVGElement::GetViewportElement(nsIDOMSVGElement * *aViewportElement)
{
  *aViewportElement = nsnull;
  nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement;
  nsresult rv = GetOwnerSVGElement(getter_AddRefs(SVGSVGElement));
  NS_ENSURE_SUCCESS(rv,rv);
  if (SVGSVGElement) {
    nsCOMPtr<nsIDOMSVGElement> SVGElement = do_QueryInterface(SVGSVGElement);
    *aViewportElement = SVGElement;
    NS_IF_ADDREF(*aViewportElement);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGElement::WillModifySVGObservable(nsISVGValue* observable,
                                      nsISVGValue::modificationType aModType)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGElement::DidModifySVGObservable(nsISVGValue* aObservable,
                                     nsISVGValue::modificationType aModType)
{
  // Return without setting DOM attributes as markup attributes if the
  // attribute's element is being inserted into an SVG document fragment,
  // which provides a context which percentage lengths are relative to.
  // Bug 274886
  if (aModType == nsISVGValue::mod_context)
    return NS_OK;

  // Return without setting DOM attribute 
  if (mSuppressNotification)
    return NS_OK;

  PRUint32 i, count = mMappedAttributes.AttrCount();
  const nsAttrValue* attrValue = nsnull;
  for (i = 0; i < count; ++i) {
    attrValue = mMappedAttributes.AttrAt(i);
    if (attrValue->GetSVGValue() == aObservable) {
      break;
    }
  }

  if (i == count) {
    NS_NOTREACHED("unknown nsISVGValue");

    return NS_ERROR_UNEXPECTED;
  }

  const nsAttrName* attrName = mMappedAttributes.AttrNameAt(i);
  PRBool modification = PR_FALSE;
  PRBool hasListeners =
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  if (hasListeners || IsInDoc()) {
    modification = !!mAttrsAndChildren.GetAttr(attrName->LocalName(),
                                               attrName->NamespaceID());
  }

  nsAttrValue newValue(aObservable);

  return SetAttrAndNotify(attrName->NamespaceID(), attrName->LocalName(),
                          attrName->GetPrefix(), EmptyString(), newValue,
                          modification, hasListeners, PR_TRUE, nsnull);
}

//----------------------------------------------------------------------
// Implementation Helpers:

PRBool
nsSVGElement::IsEventName(nsIAtom* aName)
{
  return PR_FALSE;
}

void
nsSVGElement::UpdateContentStyleRule()
{
  NS_ASSERTION(!mContentStyleRule, "we already have a content style rule");
  
  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    NS_ERROR("SVG element without owner document");
    return;
  }
  
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsIURI *docURI = doc->GetDocumentURI();
  nsICSSLoader* cssLoader = doc->CSSLoader();

  nsCSSDeclaration* declaration = nsnull;
  nsCOMPtr<nsICSSParser> parser;

  nsresult rv = NS_OK; 

  PRUint32 attrCount = mAttrsAndChildren.AttrCount();
  for (PRUint32 i = 0; i < attrCount; ++i) {
    const nsAttrName* attrName = mAttrsAndChildren.AttrNameAt(i);
    if (!attrName->IsAtom() || !IsAttributeMapped(attrName->Atom()))
      continue;

    if (!declaration) {
      // Create the nsCSSDeclaration.
      declaration = new nsCSSDeclaration();
      if (!declaration) {
        NS_WARNING("Failed to allocate nsCSSDeclaration");
        return;
      }
      if (!declaration->InitializeEmpty()) {
        NS_WARNING("could not initialize nsCSSDeclaration");
        declaration->RuleAbort();  // deletes declaration
        return;
      }

      // Try to fetch the CSS Parser from the document.
      rv = cssLoader->GetParserFor(nsnull, getter_AddRefs(parser));
      if (NS_FAILED(rv)) {
        NS_WARNING("failed to get a css parser");
        declaration->RuleAbort();  // deletes declaration
        return;
      }

      // SVG and CSS differ slightly in their interpretation of some of
      // the attributes.  SVG allows attributes of the form: font-size="5" 
      // (style="font-size: 5" if using a style attribute)
      // where CSS requires units: font-size="5pt" (style="font-size: 5pt")
      // Set a flag to pass information to the parser so that we can use
      // the CSS parser to parse the font-size attribute.  Note that this
      // does *not* affect the use of CSS stylesheets, which will still
      // require units.
      parser->SetSVGMode(PR_TRUE);
    }

    nsAutoString name;
    attrName->Atom()->ToString(name);

    nsAutoString value;
    mAttrsAndChildren.AttrAt(i)->ToString(value);

    PRBool changed;
    parser->ParseProperty(nsCSSProps::LookupProperty(name), value,
                          docURI, baseURI, NodePrincipal(),
                          declaration, &changed);
  }

  if (declaration) {
    rv = NS_NewCSSStyleRule(getter_AddRefs(mContentStyleRule), nsnull, declaration);
    if (NS_FAILED(rv)) {
      NS_WARNING("could not create contentstylerule");
      declaration->RuleAbort();  // deletes declaration
    }

    // Recycle the parser
    parser->SetSVGMode(PR_FALSE);
    cssLoader->RecycleParser(parser);
  }
}

nsISVGValue*
nsSVGElement::GetMappedAttribute(PRInt32 aNamespaceID, nsIAtom* aName)
{
  const nsAttrValue* attrVal = mMappedAttributes.GetAttr(aName, aNamespaceID);
  if (!attrVal)
    return nsnull;

  return attrVal->GetSVGValue();
}

nsresult
nsSVGElement::AddMappedSVGValue(nsIAtom* aName, nsISupports* aValue,
                                PRInt32 aNamespaceID)
{
  nsresult rv;
  nsCOMPtr<nsISVGValue> svg_value = do_QueryInterface(aValue);
  svg_value->AddObserver(this);
  nsAttrValue attrVal(svg_value);

  if (aNamespaceID == kNameSpaceID_None) {
    rv = mMappedAttributes.SetAndTakeAttr(aName, attrVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    nsCOMPtr<nsINodeInfo> ni;
    ni = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, nsnull,
                                                   aNamespaceID);
    NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

    rv = mMappedAttributes.SetAndTakeAttr(ni, attrVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
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

  return aAttr;
}

nsSVGSVGElement *
nsSVGElement::GetCtx()
{
  nsCOMPtr<nsIDOMSVGSVGElement> svg;
  GetOwnerSVGElement(getter_AddRefs(svg));
  return static_cast<nsSVGSVGElement*>(svg.get());
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
nsSVGElement::DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  LengthAttributesInfo info = GetLengthInfo();

  NS_ASSERTION(info.mLengthCount > 0,
               "DidChangeLength on element with no length attribs");

  NS_ASSERTION(aAttrEnum < info.mLengthCount, "aAttrEnum out of range");

  nsAutoString newStr;
  info.mLengths[aAttrEnum].GetBaseValueString(newStr);

  SetAttr(kNameSpaceID_None, *info.mLengthInfo[aAttrEnum].mName,
          newStr, PR_TRUE);
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
nsSVGElement::DidChangeNumber(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  NumberAttributesInfo info = GetNumberInfo();

  NS_ASSERTION(info.mNumberCount > 0,
               "DidChangeNumber on element with no number attribs");

  NS_ASSERTION(aAttrEnum < info.mNumberCount, "aAttrEnum out of range");

  nsAutoString newStr;
  info.mNumbers[aAttrEnum].GetBaseValueString(newStr);

  SetAttr(kNameSpaceID_None, *info.mNumberInfo[aAttrEnum].mName,
          newStr, PR_TRUE);
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
nsSVGElement::DidChangeInteger(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  IntegerAttributesInfo info = GetIntegerInfo();

  NS_ASSERTION(info.mIntegerCount > 0,
               "DidChangeInteger on element with no integer attribs");

  NS_ASSERTION(aAttrEnum < info.mIntegerCount, "aAttrEnum out of range");

  nsAutoString newStr;
  info.mIntegers[aAttrEnum].GetBaseValueString(newStr);

  SetAttr(kNameSpaceID_None, *info.mIntegerInfo[aAttrEnum].mName,
          newStr, PR_TRUE);
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
nsSVGElement::DidChangeAngle(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  AngleAttributesInfo info = GetAngleInfo();

  NS_ASSERTION(info.mAngleCount > 0,
               "DidChangeAngle on element with no angle attribs");

  NS_ASSERTION(aAttrEnum < info.mAngleCount, "aAttrEnum out of range");

  nsAutoString newStr;
  info.mAngles[aAttrEnum].GetBaseValueString(newStr);

  SetAttr(kNameSpaceID_None, *info.mAngleInfo[aAttrEnum].mName,
          newStr, PR_TRUE);
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
nsSVGElement::DidChangeBoolean(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  BooleanAttributesInfo info = GetBooleanInfo();

  NS_ASSERTION(info.mBooleanCount > 0,
               "DidChangeBoolean on element with no boolean attribs");

  NS_ASSERTION(aAttrEnum < info.mBooleanCount, "aAttrEnum out of range");

  nsAutoString newStr;
  info.mBooleans[aAttrEnum].GetBaseValueString(newStr);

  SetAttr(kNameSpaceID_None, *info.mBooleanInfo[aAttrEnum].mName,
          newStr, PR_TRUE);
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
nsSVGElement::DidChangeEnum(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  EnumAttributesInfo info = GetEnumInfo();

  NS_ASSERTION(info.mEnumCount > 0,
               "DidChangeEnum on element with no enum attribs");

  NS_ASSERTION(aAttrEnum < info.mEnumCount, "aAttrEnum out of range");

  nsAutoString newStr;
  info.mEnums[aAttrEnum].GetBaseValueString(newStr, this);

  SetAttr(kNameSpaceID_None, *info.mEnumInfo[aAttrEnum].mName,
          newStr, PR_TRUE);
}

nsSVGPreserveAspectRatio *
nsSVGElement::GetPreserveAspectRatio()
{
  return nsnull;
}

void
nsSVGElement::DidChangePreserveAspectRatio(PRBool aDoSetAttr)
{
  if (!aDoSetAttr)
    return;

  nsSVGPreserveAspectRatio *preserveAspectRatio = GetPreserveAspectRatio();

  NS_ASSERTION(preserveAspectRatio,
               "DidChangePreserveAspectRatio on element with no preserveAspectRatio attrib");

  nsAutoString newStr;
  preserveAspectRatio->GetBaseValueString(newStr);

  SetAttr(kNameSpaceID_None, nsGkAtoms::preserveAspectRatio,
          newStr, PR_TRUE);
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
          *info.mStringInfo[aAttrEnum].mName, aValue, PR_TRUE);
}

nsresult
nsSVGElement::ParseNumberOptionalNumber(const nsAString& aValue,
                                        PRUint32 aIndex1, PRUint32 aIndex2)
{
  NS_ConvertUTF16toUTF8 value(aValue);
  const char *str = value.get();

  if (NS_IsAsciiWhitespace(*str))
    return NS_ERROR_FAILURE;

  char *rest;
  float x = float(PR_strtod(str, &rest));
  float y = x;

  if (str == rest) {
    //first value was illformed
    return NS_ERROR_FAILURE;
  }
  
  if (*rest != '\0') {
    while (NS_IsAsciiWhitespace(*rest)) {
      ++rest;
    }
    if (*rest == ',') {
      ++rest;
    }

    y = float(PR_strtod(rest, &rest));
    if (*rest != '\0') {
      //second value was illformed or there was trailing content
      return NS_ERROR_FAILURE;
    }
  }

  NumberAttributesInfo numberInfo = GetNumberInfo();

  numberInfo.mNumbers[aIndex1].SetBaseValue(x, this, PR_FALSE);
  numberInfo.mNumbers[aIndex2].SetBaseValue(y, this, PR_FALSE);
  return NS_OK;
}

nsresult
nsSVGElement::ParseIntegerOptionalInteger(const nsAString& aValue,
                                          PRUint32 aIndex1, PRUint32 aIndex2)
{
  NS_ConvertUTF16toUTF8 value(aValue);
  const char *str = value.get();

  if (NS_IsAsciiWhitespace(*str))
    return NS_ERROR_FAILURE;

  char *rest;
  PRInt32 x = strtol(str, &rest, 10);
  PRInt32 y = x;

  if (str == rest) {
    //first value was illformed
    return NS_ERROR_FAILURE;
  }
  
  if (*rest != '\0') {
    while (NS_IsAsciiWhitespace(*rest)) {
      ++rest;
    }
    if (*rest == ',') {
      ++rest;
    }

    y = strtol(rest, &rest, 10);
    if (*rest != '\0') {
      //second value was illformed or there was trailing content
      return NS_ERROR_FAILURE;
    }
  }

  IntegerAttributesInfo integerInfo = GetIntegerInfo();

  integerInfo.mIntegers[aIndex1].SetBaseValue(x, this, PR_FALSE);
  integerInfo.mIntegers[aIndex2].SetBaseValue(y, this, PR_FALSE);

  return NS_OK;
}

nsresult
nsSVGElement::ReportAttributeParseFailure(nsIDocument* aDocument,
                                          nsIAtom* aAttribute,
                                          const nsAString& aValue)
{
  nsAutoString attributeName;
  aAttribute->ToString(attributeName);
  const nsAFlatString& attributeValue = PromiseFlatString(aValue);
  const PRUnichar *strings[] = { attributeName.get(), attributeValue.get() };
  return nsSVGUtils::ReportToConsole(aDocument,
                                     "AttributeParseWarning",
                                     strings, NS_ARRAY_LENGTH(strings));
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
    AddScriptEventListener(GetEventNameForAttr(attr), value, PR_TRUE);
  }
}

#ifdef MOZ_SMIL
nsISMILAttr*
nsSVGElement::GetAnimatedAttr(const nsIAtom* aName)
{
  // Lengths:
  LengthAttributesInfo info = GetLengthInfo();
  for (PRUint32 i = 0; i < info.mLengthCount; i++) {
    if (aName == *info.mLengthInfo[i].mName) {
      return info.mLengths[i].ToSMILAttr(this);
    }
  }

  return nsnull;
}
#endif // MOZ_SMIL
