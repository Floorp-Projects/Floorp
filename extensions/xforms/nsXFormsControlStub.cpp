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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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

#include "nsIModelElementPrivate.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsControlStub.h"
#include "nsXFormsMDGEngine.h"

#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathResult.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDocument.h"
#include "nsXFormsModelElement.h"

/** This class is used to generate xforms-hint and xforms-help events.*/
class nsXFormsHintHelpListener : public nsIDOMEventListener {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
};

NS_IMPL_ISUPPORTS1(nsXFormsHintHelpListener, nsIDOMEventListener)

NS_IMETHODIMP
nsXFormsHintHelpListener::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!aEvent)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetCurrentTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(target));
  if (nsXFormsUtils::EventHandlingAllowed(aEvent, targetNode)) {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
    if (keyEvent) {
      PRUint32 code = 0;
      keyEvent->GetKeyCode(&code);
      if (code == nsIDOMKeyEvent::DOM_VK_F1)
        nsXFormsUtils::DispatchEvent(targetNode, eEvent_Help);
    } else {
      nsXFormsUtils::DispatchEvent(targetNode, eEvent_Hint);
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsControlStub,
                             nsXFormsXMLVisualStub,
                             nsIXFormsContextControl,
                             nsIXFormsControl)

NS_IMETHODIMP
nsXFormsControlStub::GetBoundNode(nsIDOMNode **aBoundNode)
{
  NS_IF_ADDREF(*aBoundNode = mBoundNode);
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsControlStub::GetDependencies(nsCOMArray<nsIDOMNode> **aDependencies)
{
  if (aDependencies)
    *aDependencies = &mDependencies;
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsControlStub::GetElement(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mElement);
  return NS_OK;  
}

void
nsXFormsControlStub::RemoveIndexListeners()
{
  if (!mIndexesUsed.Count())
    return;

  for (PRInt32 i = 0; i < mIndexesUsed.Count(); ++i) {
    nsCOMPtr<nsIXFormsRepeatElement> rep = mIndexesUsed[i];
    rep->RemoveIndexUser(this);
  }

  mIndexesUsed.Clear();
}

NS_IMETHODIMP
nsXFormsControlStub::ResetBoundNode(const nsString     &aBindAttribute,
                                    PRUint16            aResultType,
                                    nsIDOMXPathResult **aResult)
{
  // Clear existing bound node, etc.
  mBoundNode = nsnull;
  mDependencies.Clear();
  RemoveIndexListeners();

  if (!mHasParent || !mBindAttrsCount)
    return NS_OK;

  nsCOMPtr<nsIDOMXPathResult> result;
  nsresult rv =
    ProcessNodeBinding(aBindAttribute,
                       aResultType,
                       getter_AddRefs(result));

  if (NS_FAILED(rv)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("controlBindError"), mElement);
    return rv;
  }
  
  if (!result)
    return NS_OK;
    
  // Get context node, if any  
  result->GetSingleNodeValue(getter_AddRefs(mBoundNode));

  if (mBoundNode && mModel) {
    mModel->SetStates(this, mBoundNode);
  }

  if (aResult) {
    *aResult = nsnull;
    result.swap(*aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::Bind()
{
  return ResetBoundNode(NS_LITERAL_STRING("ref"),
                        nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE);
}

NS_IMETHODIMP
nsXFormsControlStub::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}
  

nsresult
nsXFormsControlStub::ProcessNodeBinding(const nsString          &aBindingAttr,
                                        PRUint16                 aResultType,
                                        nsIDOMXPathResult      **aResult,
                                        nsIModelElementPrivate **aModel)
{
  nsStringArray indexesUsed;

  // let's not go through all of this rigamarol if we don't have a chance
  // in heck of binding anyhow.  Check to see if the models will be receptive
  // to some binding.  readyForBindProperty is set when they are.  Make sure
  // to return NS_OK so that we don't start complaining about binding
  // failures in this situation.

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) {
    return NS_OK;
  }
  nsIDocument *test = NS_STATIC_CAST(nsIDocument *,
                        doc->GetProperty(nsXFormsAtoms::readyForBindProperty));
  if (!test) {
    nsXFormsModelElement::DeferElementBind(domDoc, this);
    return NS_OK;
  }

  nsresult rv;
  rv = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                          kElementFlags,
                                          aBindingAttr,
                                          EmptyString(),
                                          aResultType,
                                          getter_AddRefs(mModel),
                                          aResult,
                                          &mDependencies,
                                          &indexesUsed);
  NS_ENSURE_STATE(mModel);
  
  mModel->AddFormControl(this);
  if (aModel)
    NS_ADDREF(*aModel = mModel);

  if (NS_SUCCEEDED(rv) && indexesUsed.Count()) {
    // add index listeners on repeat elements
    
    for (PRInt32 i = 0; i < indexesUsed.Count(); ++i) {
      // Find the repeat element and add |this| as a listener
      nsCOMPtr<nsIDOMElement> repElem;
      domDoc->GetElementById(*(indexesUsed[i]), getter_AddRefs(repElem));
      nsCOMPtr<nsIXFormsRepeatElement> rep(do_QueryInterface(repElem));
      if (!rep)
        continue;

      rv = rep->AddIndexUser(this);
      if (NS_FAILED(rv)) {
        return rv;
      }
      rv = mIndexesUsed.AppendObject(rep);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return rv;
}

void
nsXFormsControlStub::ResetHelpAndHint(PRBool aInitialize)
{
  nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(mElement));
  if (!targ)
    return;

  NS_NAMED_LITERAL_STRING(mouseover, "mouseover");
  NS_NAMED_LITERAL_STRING(focus, "focus");
  NS_NAMED_LITERAL_STRING(keypress, "keypress");

  if (mEventListener) {
    targ->RemoveEventListener(mouseover, mEventListener, PR_TRUE);
    targ->RemoveEventListener(focus, mEventListener, PR_TRUE);
    targ->RemoveEventListener(keypress, mEventListener, PR_TRUE);
    mEventListener = nsnull;
  }

  if (aInitialize) {
    mEventListener = new nsXFormsHintHelpListener();
    if (!mEventListener)
      return;

    targ->AddEventListener(mouseover, mEventListener, PR_TRUE);
    targ->AddEventListener(focus, mEventListener, PR_TRUE);
    targ->AddEventListener(keypress, mEventListener, PR_TRUE);
  }
}

PRBool
nsXFormsControlStub::GetReadOnlyState()
{
  PRBool res = PR_FALSE;
  if (mElement) {
    mElement->HasAttribute(NS_LITERAL_STRING("read-only"), &res);
  }  
  return res;
}

PRBool
nsXFormsControlStub::GetRelevantState()
{
  PRBool res = PR_FALSE;
  if (mElement) {
    mElement->HasAttribute(NS_LITERAL_STRING("enabled"), &res);
  }  
  return res;
}

NS_IMETHODIMP
nsXFormsControlStub::HandleDefault(nsIDOMEvent *aEvent,
                                   PRBool      *aHandled)
{
  NS_ENSURE_ARG(aHandled);
  *aHandled = PR_FALSE;

  if (nsXFormsUtils::EventHandlingAllowed(aEvent, mElement)) {

    // Check that we are the target of the event
    nsCOMPtr<nsIDOMEventTarget> target;
    aEvent->GetTarget(getter_AddRefs(target));
    nsCOMPtr<nsIDOMElement> targetE(do_QueryInterface(target));
    if (targetE && targetE != mElement) {
      return NS_OK;
    }

    // Handle event
    nsAutoString type;
    aEvent->GetType(type);

    if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Focus].name)) {
      TryFocus(aHandled);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(kStandardNotificationMask);

  aWrapper->GetElementNode(getter_AddRefs(mElement));
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  ResetHelpAndHint(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::OnDestroyed()
{
  ResetHelpAndHint(PR_FALSE);
  RemoveIndexListeners();

  if (mModel) {
    mModel->RemoveFormControl(this);
    mModel = nsnull;
  }
  mElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  // We need to re-evaluate our instance data binding when our document
  // changes, since our context can change
  if (aNewDocument) {
    Bind();
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::ParentChanged(nsIDOMElement *aNewParent)
{
  mHasParent = aNewParent != nsnull;
  // We need to re-evaluate our instance data binding when our parent changes,
  // since xmlns declarations or our context could have changed.
  if (mHasParent) {
    Bind();
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  MaybeRemoveFromModel(aName, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  MaybeBindAndRefresh(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::WillRemoveAttribute(nsIAtom *aName)
{
  MaybeRemoveFromModel(aName, EmptyString());
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::AttributeRemoved(nsIAtom *aName)
{
  MaybeBindAndRefresh(aName);
  return NS_OK;
}

// nsIXFormsContextControl

NS_IMETHODIMP
nsXFormsControlStub::SetContextNode(nsIDOMNode *aContextNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXFormsControlStub::GetContext(nsAString      &aModelID,
                                nsIDOMNode    **aContextNode,
                                PRInt32        *aContextPosition,
                                PRInt32        *aContextSize)
{
  NS_ENSURE_ARG(aContextSize);
  NS_ENSURE_ARG(aContextPosition);

  *aContextPosition = 1;
  *aContextSize = 1;

  if (mBoundNode && aContextNode) {
    CallQueryInterface(mBoundNode, aContextNode); // addrefs
    NS_ASSERTION(*aContextNode, "could not QI context node from bound node?");
  }

  ///
  /// @todo expensive to run this
  nsCOMPtr<nsIDOMElement> model = do_QueryInterface(mModel);
  if (model) {
    model->GetAttribute(NS_LITERAL_STRING("id"), aModelID);
  }
  
  return NS_OK;
}

void
nsXFormsControlStub::ResetProperties()
{
  if (!mElement) {
    return;
  }
  
  ///
  /// @todo removes the attributes we use, until bug 271720 is landed (XXX)
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(kStateAttributes); ++i) {
    mElement->RemoveAttribute(kStateAttributes[i]);
  }
}

void
nsXFormsControlStub::AddRemoveSNBAttr(nsIAtom *aName, const nsAString &aValue) 
{
  nsAutoString attrStr, attrValue;
  aName->ToString(attrStr);
  mElement->GetAttribute(attrStr, attrValue);

  // if we are setting a single node binding attribute that we don't already
  // have, bump the count.
  if (!aValue.IsEmpty() && attrValue.IsEmpty()) {
    ++mBindAttrsCount;
  } else if (!attrValue.IsEmpty()) { 
    // if we are setting a currently existing binding attribute to have an
    // empty value, treat it like the binding attr is being removed.
    --mBindAttrsCount;
    NS_ASSERTION(mBindAttrsCount>=0, "bad mojo!  mBindAttrsCount < 0!");
    if (!mBindAttrsCount) {
      ResetProperties();
    }
  }
}

void
nsXFormsControlStub::MaybeBindAndRefresh(nsIAtom *aName)
{
  if (aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref  ||
      aName == nsXFormsAtoms::model) {

    Bind();
    Refresh();
  }

}

void
nsXFormsControlStub::MaybeRemoveFromModel(nsIAtom         *aName, 
                                          const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref) {
    if (mModel)
      mModel->RemoveFormControl(this);

    if (aName != nsXFormsAtoms::model) {
      AddRemoveSNBAttr(aName, aValue);
    }
  }
}
