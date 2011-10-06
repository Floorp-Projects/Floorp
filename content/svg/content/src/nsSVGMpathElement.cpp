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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

#include "nsSVGMpathElement.h"
#include "nsAutoPtr.h"
#include "nsDebug.h"
#include "nsSVGPathElement.h"
#include "nsSVGAnimateMotionElement.h"

using namespace mozilla::dom;

nsSVGElement::StringInfo nsSVGMpathElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_FALSE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Mpath)

// Cycle collection magic -- based on nsSVGUseElement
NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGMpathElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsSVGMpathElement,
                                                nsSVGMpathElementBase)
  tmp->UnlinkHrefTarget(PR_FALSE);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsSVGMpathElement,
                                                  nsSVGMpathElementBase)
  tmp->mHrefTarget.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGMpathElement,nsSVGMpathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGMpathElement,nsSVGMpathElementBase)

DOMCI_NODE_DATA(SVGMpathElement, nsSVGMpathElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsSVGMpathElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGMpathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,  nsIDOMSVGURIReference,
                           nsIDOMSVGMpathElement, nsIMutationObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGMpathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMpathElementBase)

// Constructor
#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
// We can ignore that warning because we know that mHrefTarget's constructor 
// doesn't dereference the pointer passed to it.
#pragma warning(push)
#pragma warning(disable:4355)
#endif
nsSVGMpathElement::nsSVGMpathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGMpathElementBase(aNodeInfo),
    mHrefTarget(this)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
{
}

nsSVGMpathElement::~nsSVGMpathElement()
{
  UnlinkHrefTarget(PR_FALSE);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGMpathElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGMpathElement::GetHref(nsIDOMSVGAnimatedString** aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGMpathElement::BindToTree(nsIDocument* aDocument,
                              nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  NS_ABORT_IF_FALSE(!mHrefTarget.get(),
                    "Shouldn't have href-target yet "
                    "(or it should've been cleared)");
  nsresult rv = nsSVGMpathElementBase::BindToTree(aDocument, aParent,
                                                  aBindingParent,
                                                  aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv,rv);

  if (aDocument) {
    const nsAttrValue* hrefAttrValue =
      mAttrsAndChildren.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
    if (hrefAttrValue) {
      UpdateHrefTarget(aParent, hrefAttrValue->GetStringValue());
    }
  }

  return NS_OK;
}

void
nsSVGMpathElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  UnlinkHrefTarget(PR_TRUE);
  nsSVGMpathElementBase::UnbindFromTree(aDeep, aNullParent);
}

bool
nsSVGMpathElement::ParseAttribute(PRInt32 aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  bool returnVal =
    nsSVGMpathElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                          aValue, aResult);
  if (aNamespaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href &&
      IsInDoc()) {
    // NOTE: If we fail the IsInDoc call, it's ok -- we'll update the target
    // on next BindToTree call.
    UpdateHrefTarget(GetParent(), aValue);
  }
  return returnVal;
}

nsresult
nsSVGMpathElement::UnsetAttr(PRInt32 aNamespaceID,
                             nsIAtom* aAttribute, bool aNotify)
{
  nsresult rv = nsSVGMpathElementBase::UnsetAttr(aNamespaceID, aAttribute,
                                                 aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNamespaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {
    UnlinkHrefTarget(PR_TRUE);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGMpathElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
nsSVGMpathElement::AttributeChanged(nsIDocument* aDocument,
                                    Element* aElement,
                                    PRInt32 aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    PRInt32 aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::d) {
      NotifyParentOfMpathChange(GetParent());
    }
  }
}

//----------------------------------------------------------------------
// Public helper methods

nsSVGPathElement*
nsSVGMpathElement::GetReferencedPath()
{
  if (!HasAttr(kNameSpaceID_XLink, nsGkAtoms::href)) {
    NS_ABORT_IF_FALSE(!mHrefTarget.get(),
                      "We shouldn't have an xlink:href target "
                      "if we don't have an xlink:href attribute");
    return nsnull;
  }

  nsIContent* genericTarget = mHrefTarget.get();
  if (genericTarget &&
      genericTarget->Tag() == nsGkAtoms::path) {
    return static_cast<nsSVGPathElement*>(genericTarget);
  }
  return nsnull;
}

//----------------------------------------------------------------------
// Protected helper methods

void
nsSVGMpathElement::UpdateHrefTarget(nsIContent* aParent,
                                    const nsAString& aHrefStr)
{
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI),
                                            aHrefStr, GetOwnerDoc(), baseURI);

  // Stop observing old target (if any)
  if (mHrefTarget.get()) {
    mHrefTarget.get()->RemoveMutationObserver(this);
  }

  if (aParent) {
    // Pass in |aParent| instead of |this| -- first argument is only used
    // for a call to GetCurrentDoc(), and |this| might not have a current
    // document yet (if our caller is BindToTree).
    mHrefTarget.Reset(aParent, targetURI);
  } else {
    // if we don't have a parent, then there's no animateMotion element
    // depending on our target, so there's no point tracking it right now.
    mHrefTarget.Unlink();
  }

  // Start observing new target (if any)
  if (mHrefTarget.get()) {
    mHrefTarget.get()->AddMutationObserver(this);
  }

  NotifyParentOfMpathChange(aParent);
}

void
nsSVGMpathElement::UnlinkHrefTarget(bool aNotifyParent)
{
  // Stop observing old target (if any)
  if (mHrefTarget.get()) {
    mHrefTarget.get()->RemoveMutationObserver(this);
  }
  mHrefTarget.Unlink();

  if (aNotifyParent) {
    NotifyParentOfMpathChange(GetParent());
  }
}

void
nsSVGMpathElement::NotifyParentOfMpathChange(nsIContent* aParent)
{
  if (aParent &&
      aParent->GetNameSpaceID() == kNameSpaceID_SVG &&
      aParent->Tag() == nsGkAtoms::animateMotion) {

    nsSVGAnimateMotionElement* animateMotionParent =
      static_cast<nsSVGAnimateMotionElement*>(aParent);

    animateMotionParent->MpathChanged();
    AnimationNeedsResample();
  }
}
