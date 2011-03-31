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

  nsIDocument* doc = GetOwnerDoc();
  NS_ASSERTION(doc, "If we're animating we should have an owner");
  if (doc) {
    nsIPresShell* shell = doc->GetShell();
    if (shell) {
      shell->RestyleForAnimation(this, eRestyle_Self);
    }
  }

  nsSVGStylableElementBase::DidAnimateClass();
}
