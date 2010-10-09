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
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *   Chris Double  <chris.double@double.co.nz>
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

#include "nsSVGAnimationElement.h"
#include "nsSVGSVGElement.h"
#include "nsSMILTimeContainer.h"
#include "nsSMILAnimationController.h"
#include "nsSMILAnimationFunction.h"
#include "nsISMILAttr.h"

using namespace mozilla::dom;

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAnimationElement, nsSVGAnimationElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAnimationElement, nsSVGAnimationElementBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGAnimationElement)
  NS_INTERFACE_MAP_ENTRY(nsISMILAnimationElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElementTimeControl)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAnimationElementBase)

// Cycle collection magic -- based on nsSVGUseElement
NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGAnimationElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsSVGAnimationElement,
                                                nsSVGAnimationElementBase)
  tmp->mHrefTarget.Unlink();
  tmp->mTimedElement.Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsSVGAnimationElement,
                                                  nsSVGAnimationElementBase)
  tmp->mHrefTarget.Traverse(&cb);
  tmp->mTimedElement.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

//----------------------------------------------------------------------
// Implementation

#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
// We can ignore that warning because we know that mHrefTarget's constructor 
// doesn't dereference the pointer passed to it.
#pragma warning(push)
#pragma warning(disable:4355)
#endif
nsSVGAnimationElement::nsSVGAnimationElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGAnimationElementBase(aNodeInfo),
    mHrefTarget(this),
    mTimedDocumentRoot(nsnull)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
{
}

nsresult
nsSVGAnimationElement::Init()
{
  nsresult rv = nsSVGAnimationElementBase::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mTimedElement.SetAnimationElement(this);
  AnimationFunction().SetAnimationElement(this);
  mTimedElement.SetTimeClient(&AnimationFunction());

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

const Element&
nsSVGAnimationElement::AsElement() const
{
  return *this;
}

Element&
nsSVGAnimationElement::AsElement()
{
  return *this;
}

const nsAttrValue*
nsSVGAnimationElement::GetAnimAttr(nsIAtom* aName) const
{
  return mAttrsAndChildren.GetAttr(aName, kNameSpaceID_None);
}

PRBool
nsSVGAnimationElement::GetAnimAttr(nsIAtom* aAttName,
                                   nsAString& aResult) const
{
  return GetAttr(kNameSpaceID_None, aAttName, aResult);
}

PRBool
nsSVGAnimationElement::HasAnimAttr(nsIAtom* aAttName) const
{
  return HasAttr(kNameSpaceID_None, aAttName);
}

Element*
nsSVGAnimationElement::GetTargetElementContent()
{
  if (HasAttr(kNameSpaceID_XLink, nsGkAtoms::href)) {
    return mHrefTarget.get();
  }
  NS_ABORT_IF_FALSE(!mHrefTarget.get(),
                    "We shouldn't have an xlink:href target "
                    "if we don't have an xlink:href attribute");

  // No "xlink:href" attribute --> I should target my parent.
  return nsSVGUtils::GetParentElement(this);
}

PRBool
nsSVGAnimationElement::GetTargetAttributeName(PRInt32 *aNamespaceID,
                                              nsIAtom **aLocalName) const
{
  const nsAttrValue* nameAttr
    = mAttrsAndChildren.GetAttr(nsGkAtoms::attributeName);

  if (!nameAttr)
    return PR_FALSE;

  NS_ASSERTION(nameAttr->Type() == nsAttrValue::eAtom,
    "attributeName should have been parsed as an atom");

  return NS_SUCCEEDED(nsContentUtils::SplitQName(
                        this, nsDependentAtomString(nameAttr->GetAtomValue()),
                        aNamespaceID, aLocalName));
}

nsSMILTargetAttrType
nsSVGAnimationElement::GetTargetAttributeType() const
{
  nsIContent::AttrValuesArray typeValues[] = { &nsGkAtoms::css,
                                               &nsGkAtoms::XML,
                                               nsnull};
  nsSMILTargetAttrType smilTypes[] = { eSMILTargetAttrType_CSS,
                                       eSMILTargetAttrType_XML };
  PRInt32 index = FindAttrValueIn(kNameSpaceID_None,
                                  nsGkAtoms::attributeType,
                                  typeValues,
                                  eCaseMatters);
  return (index >= 0) ? smilTypes[index] : eSMILTargetAttrType_auto;
}

nsSMILTimedElement&
nsSVGAnimationElement::TimedElement()
{
  return mTimedElement;
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimationElement methods

/* readonly attribute SVGElement targetElement; */
NS_IMETHODIMP
nsSVGAnimationElement::GetTargetElement(nsIDOMSVGElement** aTarget)
{
  FlushAnimations();

  // We'll just call the other GetTargetElement method, and QI to the right type
  nsIContent* targetContent = GetTargetElementContent();

  nsCOMPtr<nsIDOMSVGElement> targetSVG = do_QueryInterface(targetContent);
  NS_IF_ADDREF(*aTarget = targetSVG);

  return NS_OK;
}

/* float getStartTime() raises( DOMException ); */
NS_IMETHODIMP
nsSVGAnimationElement::GetStartTime(float* retval)
{
  FlushAnimations();

  nsSMILTimeValue startTime = mTimedElement.GetStartTime();
  if (!startTime.IsResolved())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  *retval = float(double(startTime.GetMillis()) / PR_MSEC_PER_SEC);

  return NS_OK;
}

/* float getCurrentTime(); */
NS_IMETHODIMP
nsSVGAnimationElement::GetCurrentTime(float* retval)
{
  // Not necessary to call FlushAnimations() for this

  nsSMILTimeContainer* root = GetTimeContainer();
  if (root) {
    *retval = float(double(root->GetCurrentTime()) / PR_MSEC_PER_SEC);
  } else {
    *retval = 0.f;
  }
  return NS_OK;
}

/* float getSimpleDuration() raises( DOMException ); */
NS_IMETHODIMP
nsSVGAnimationElement::GetSimpleDuration(float* retval)
{
  // Not necessary to call FlushAnimations() for this

  nsSMILTimeValue simpleDur = mTimedElement.GetSimpleDuration();
  if (!simpleDur.IsResolved()) {
    *retval = 0.f;
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  *retval = float(double(simpleDur.GetMillis()) / PR_MSEC_PER_SEC);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGAnimationElement::BindToTree(nsIDocument* aDocument,
                                  nsIContent* aParent,
                                  nsIContent* aBindingParent,
                                  PRBool aCompileEventHandlers)
{
  NS_ABORT_IF_FALSE(!mHrefTarget.get(),
                    "Shouldn't have href-target yet "
                    "(or it should've been cleared)");
  nsresult rv = nsSVGAnimationElementBase::BindToTree(aDocument, aParent,
                                                      aBindingParent,
                                                      aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv,rv);

  // XXXdholbert is ownerDOMSVG (as a check for SVG parent) still needed here?
  nsCOMPtr<nsIDOMSVGSVGElement> ownerDOMSVG;
  rv = GetOwnerSVGElement(getter_AddRefs(ownerDOMSVG));

  if (NS_FAILED(rv) || !ownerDOMSVG)
    // No use proceeding. We don't have an SVG parent (yet) so we won't be able
    // to register ourselves etc. Maybe next time we'll have more luck.
    // (This sort of situation will arise a lot when trees are being constructed
    // piece by piece via script)
    return NS_OK;

  mTimedDocumentRoot = GetTimeContainer();
  if (!mTimedDocumentRoot)
    // Timed document root hasn't been created yet. This will be created when
    // the SVG parent is bound. This happens when we create SVG trees entirely
    // by script.
    return NS_OK;

  // Add myself to the animation controller's master set of animation elements.
  if (aDocument) {
    nsSMILAnimationController *controller = aDocument->GetAnimationController();
    if (controller) {
      controller->RegisterAnimationElement(this);
    }
    const nsAttrValue* href = mAttrsAndChildren.GetAttr(nsGkAtoms::href,
                                                        kNameSpaceID_XLink);
    if (href) {
      nsAutoString hrefStr;
      href->ToString(hrefStr);

      // Pass in |aParent| instead of |this| -- first argument is only used
      // for a call to GetCurrentDoc(), and |this| might not have a current
      // document yet.
      UpdateHrefTarget(aParent, hrefStr);
    }

    mTimedElement.BindToTree(aParent);
  }

  AnimationNeedsResample();

  return NS_OK;
}

void
nsSVGAnimationElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsIDocument *doc = GetOwnerDoc();
  if (doc) {
    nsSMILAnimationController *controller = doc->GetAnimationController();
    if (controller) {
      controller->UnregisterAnimationElement(this);
    }
  }

  if (mTimedDocumentRoot) {
    mTimedDocumentRoot = nsnull;
  }

  mHrefTarget.Unlink();
  mTimedElement.DissolveReferences();

  AnimationNeedsResample();

  nsSVGAnimationElementBase::UnbindFromTree(aDeep, aNullParent);
}

PRBool
nsSVGAnimationElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    // Deal with target-related attributes here
    if (aAttribute == nsGkAtoms::attributeName ||
        aAttribute == nsGkAtoms::attributeType) {
      aResult.ParseAtom(aValue);
      AnimationNeedsResample();
      return PR_TRUE;
    }

    nsresult rv = NS_ERROR_FAILURE;

    // First let the animation function try to parse it...
    PRBool foundMatch =
      AnimationFunction().SetAttr(aAttribute, aValue, aResult, &rv);

    // ... and if that didn't recognize the attribute, let the timed element
    // try to parse it.
    if (!foundMatch) {
      foundMatch =
        mTimedElement.SetAttr(aAttribute, aValue, aResult, this, &rv);
    }

    if (foundMatch) {
      AnimationNeedsResample();
      if (NS_FAILED(rv)) {
        ReportAttributeParseFailure(GetOwnerDoc(), aAttribute, aValue);
        return PR_FALSE;
      }
      return PR_TRUE;
    }
  }

  return nsSVGAnimationElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                                   aValue, aResult);
}

nsresult
nsSVGAnimationElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                    const nsAString* aValue, PRBool aNotify)
{
  nsresult rv =
    nsSVGAnimationElementBase::AfterSetAttr(aNamespaceID, aName, aValue,
                                            aNotify);

  if (aNamespaceID != kNameSpaceID_XLink || aName != nsGkAtoms::href)
    return rv;

  if (!aValue) {
    mHrefTarget.Unlink();
    AnimationTargetChanged();
  } else if (IsInDoc()) {
    UpdateHrefTarget(this, *aValue);
  } // else: we're not yet in a document -- we'll update the target on
    // next BindToTree call.

  return rv;
}

nsresult
nsSVGAnimationElement::UnsetAttr(PRInt32 aNamespaceID,
                                 nsIAtom* aAttribute, PRBool aNotify)
{
  nsresult rv = nsSVGAnimationElementBase::UnsetAttr(aNamespaceID, aAttribute,
                                                     aNotify);
  NS_ENSURE_SUCCESS(rv,rv);

  if (aNamespaceID == kNameSpaceID_None) {
    if (AnimationFunction().UnsetAttr(aAttribute) ||
        mTimedElement.UnsetAttr(aAttribute)) {
      AnimationNeedsResample();
    }
  }

  return NS_OK;
}

PRBool
nsSVGAnimationElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eSVG | eANIMATION));
}

//----------------------------------------------------------------------
// Implementation helpers

nsSMILTimeContainer*
nsSVGAnimationElement::GetTimeContainer()
{
  nsSMILTimeContainer *result = nsnull;
  nsCOMPtr<nsIDOMSVGSVGElement> ownerDOMSVG;

  nsresult rv = GetOwnerSVGElement(getter_AddRefs(ownerDOMSVG));

  if (NS_SUCCEEDED(rv) && ownerDOMSVG) {
    nsSVGSVGElement *ownerSVG =
      static_cast<nsSVGSVGElement*>(ownerDOMSVG.get());
    result = ownerSVG->GetTimedDocumentRoot();
  }

  return result;
}

// nsIDOMElementTimeControl
/* void beginElement (); */
NS_IMETHODIMP
nsSVGAnimationElement::BeginElement(void)
{
  return BeginElementAt(0.f);
}

/* void beginElementAt (in float offset); */
NS_IMETHODIMP
nsSVGAnimationElement::BeginElementAt(float offset)
{
  // This will fail if we're not attached to a time container (SVG document
  // fragment).
  nsresult rv = mTimedElement.BeginElementAt(offset);
  if (NS_FAILED(rv))
    return rv;

  AnimationNeedsResample();

  return NS_OK;
}

/* void endElement (); */
NS_IMETHODIMP
nsSVGAnimationElement::EndElement(void)
{
  return EndElementAt(0.f);
}

/* void endElementAt (in float offset); */
NS_IMETHODIMP
nsSVGAnimationElement::EndElementAt(float offset)
{
  nsresult rv = mTimedElement.EndElementAt(offset);
  if (NS_FAILED(rv))
    return rv;

  AnimationNeedsResample();
 
  return NS_OK;
}

PRBool
nsSVGAnimationElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SMIL);
}

void
nsSVGAnimationElement::UpdateHrefTarget(nsIContent* aNodeForContext,
                                        const nsAString& aHrefStr)
{
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI),
                                            aHrefStr, GetOwnerDoc(), baseURI);
  mHrefTarget.Reset(aNodeForContext, targetURI);
  AnimationTargetChanged();
}

void
nsSVGAnimationElement::AnimationTargetChanged()
{
  mTimedElement.HandleTargetElementChange(GetTargetElementContent());
  AnimationNeedsResample();
}
