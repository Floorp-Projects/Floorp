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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsDOMCSSDeclaration.h"
#include "nsContentUtils.h"

//----------------------------------------------------------------------
// nsISupports methods

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGStylableElement::DOMAnimatedClassString, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGStylableElement::DOMAnimatedClassString)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGStylableElement::DOMAnimatedClassString)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGStylableElement::DOMAnimatedClassString)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedString)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedString)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_INHERITED(nsSVGStylableElement, nsSVGStylableElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGStylableElement, nsSVGStylableElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGStylableElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGStylable)
NS_INTERFACE_MAP_END_INHERITING(nsSVGStylableElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGStylableElement::nsSVGStylableElement(nsINodeInfo *aNodeInfo)
  : nsSVGStylableElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIContent methods

const nsAttrValue*
nsSVGStylableElement::DoGetClasses() const
{
  return GetClassAnimAttr();
}

//----------------------------------------------------------------------
// nsIDOMSVGStylable methods

/* readonly attribute nsIDOMSVGAnimatedString className; */
NS_IMETHODIMP
nsSVGStylableElement::GetClassName(nsIDOMSVGAnimatedString** aClassName)
{
  *aClassName = new DOMAnimatedClassString(this);
  NS_ENSURE_TRUE(*aClassName, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aClassName);
  return NS_OK;
}

/* readonly attribute nsIDOMCSSStyleDeclaration style; */
NS_IMETHODIMP
nsSVGStylableElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  return nsSVGStylableElementBase::GetStyle(aStyle);
}

/* nsIDOMCSSValue getPresentationAttribute (in DOMString name); */
NS_IMETHODIMP
nsSVGStylableElement::GetPresentationAttribute(const nsAString& aName,
                                               nsIDOMCSSValue** aReturn)
{
  // Let's not implement this just yet. The CSSValue interface has been
  // deprecated by the CSS WG.
  // http://lists.w3.org/Archives/Public/www-style/2003Oct/0347.html

  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsSVGElement methods

PRBool
nsSVGStylableElement::ParseAttribute(PRInt32 aNamespaceID,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::_class) {
    mClassAnimAttr = nsnull;
    // let the rest be handled in nsStyledElement
  }

  return nsSVGStylableElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                                   aValue, aResult);
}

nsresult
nsSVGStylableElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::_class) {
    mClassAnimAttr = nsnull;
  }

  return nsSVGStylableElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

//----------------------------------------------------------------------
// Methods for managing the class attribute

const nsAttrValue*
nsSVGStylableElement::GetClassAnimAttr() const
{
  if (mClassAnimAttr)
    return mClassAnimAttr;

  return mAttrsAndChildren.GetAttr(nsGkAtoms::_class, kNameSpaceID_None);
}

void
nsSVGStylableElement::GetClassBaseValString(nsAString& aResult) const
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aResult);
}

void
nsSVGStylableElement::SetClassBaseValString(const nsAString& aValue)
{
  mClassAnimAttr = nsnull;
  SetAttr(kNameSpaceID_None, nsGkAtoms::_class, aValue, PR_TRUE); 
}

void
nsSVGStylableElement::GetClassAnimValString(nsAString& aResult) const
{
  const nsAttrValue* attr = GetClassAnimAttr();

  if (!attr) {
    aResult.Truncate();
    return;
  }

  attr->ToString(aResult);
}
