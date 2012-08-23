/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsDOMCSSDeclaration.h"

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGStylableElement, nsSVGStylableElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGStylableElement, nsSVGStylableElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGStylableElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGStylable)
NS_INTERFACE_MAP_END_INHERITING(nsSVGStylableElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGStylableElement::nsSVGStylableElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGStylableElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIContent methods

const nsAttrValue*
nsSVGStylableElement::DoGetClasses() const
{
  if (mClassAttribute.IsAnimated()) {
    return mClassAnimAttr;
  }
  return nsSVGStylableElementBase::DoGetClasses();
}

bool
nsSVGStylableElement::ParseAttribute(int32_t aNamespaceID,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::_class) {
    mClassAttribute.SetBaseValue(aValue, this, false);
    aResult.ParseAtomArray(aValue);
    return true;
  }
  return nsSVGStylableElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                                  aResult);
}

nsresult
nsSVGStylableElement::UnsetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::_class) {
    mClassAttribute.Init();
  }
  return nsSVGStylableElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

//----------------------------------------------------------------------
// nsIDOMSVGStylable methods

/* readonly attribute nsIDOMSVGAnimatedString className; */
NS_IMETHODIMP
nsSVGStylableElement::GetClassName(nsIDOMSVGAnimatedString** aClassName)
{
  return mClassAttribute.ToDOMAnimatedString(aClassName, this);
}

/* readonly attribute nsIDOMCSSStyleDeclaration style; */
NS_IMETHODIMP
nsSVGStylableElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  nsresult rv;
  *aStyle = GetStyle(&rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ADDREF(*aStyle);
  return NS_OK;
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

void
nsSVGStylableElement::DidAnimateClass()
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

nsISMILAttr*
nsSVGStylableElement::GetAnimatedAttr(int32_t aNamespaceID, nsIAtom* aName)
{
  if (aNamespaceID == kNameSpaceID_None && 
      aName == nsGkAtoms::_class) {
    return mClassAttribute.ToSMILAttr(this);
  }
  return nsSVGStylableElementBase::GetAnimatedAttr(aNamespaceID, aName);
}
