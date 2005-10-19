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
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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
#include "nsXFormsControlStub.h"
#include "nsXFormsMDGEngine.h"

#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDocument.h"
#include "nsXFormsModelElement.h"
#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIEventStateManager.h"

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
      if (code == nsIDOMKeyEvent::DOM_VK_F1) {
        PRBool defaultEnabled = PR_TRUE;
        nsresult rv = nsXFormsUtils::DispatchEvent(targetNode, eEvent_Help,
                                                   &defaultEnabled);

        // If the xforms event's default behavior was disabled, prevent the DOM
        // event's default action as well.  This means that the F1 key was
        // handled (a help dialog was shown) and we don't want the browser to
        // show it's help dialog.
        if (NS_SUCCEEDED(rv) && !defaultEnabled)
          aEvent->PreventDefault();
      }
    } else {
      nsAutoString type;
      aEvent->GetType(type);
      nsXFormsUtils::DispatchEvent(targetNode,
                                   (type.EqualsLiteral("mouseover") ||
                                    type.EqualsLiteral("focus"))
                                   ? eEvent_Hint : eEvent_MozHintOff);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStubBase::GetBoundNode(nsIDOMNode **aBoundNode)
{
  NS_IF_ADDREF(*aBoundNode = mBoundNode);
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsControlStubBase::GetDependencies(nsCOMArray<nsIDOMNode> **aDependencies)
{
  if (aDependencies)
    *aDependencies = &mDependencies;
  return NS_OK;  
}

NS_IMETHODIMP
nsXFormsControlStubBase::GetElement(nsIDOMElement **aElement)
{
  NS_IF_ADDREF(*aElement = mElement);
  return NS_OK;  
}

void
nsXFormsControlStubBase::RemoveIndexListeners()
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
nsXFormsControlStubBase::ResetBoundNode(const nsString     &aBindAttribute,
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
  } else if (mModel) {
    // we should have been successful.  Must be pointing to a node that
    // doesn't exist in the instance document.  Disable the control
    // per 4.2.2 in the spec

    nsCOMPtr<nsIXTFElementWrapper> xtfWrap(do_QueryInterface(mElement));
    NS_ENSURE_STATE(xtfWrap);
    xtfWrap->SetIntrinsicState(NS_EVENT_STATE_DISABLED);

    // Dispatch event
    nsXFormsUtils::DispatchEvent(mElement, eEvent_Disabled);
  }

  if (aResult) {
    *aResult = nsnull;
    result.swap(*aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStubBase::Bind()
{
  return ResetBoundNode(NS_LITERAL_STRING("ref"),
                        nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE);
}

NS_IMETHODIMP
nsXFormsControlStubBase::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}
  
NS_IMETHODIMP
nsXFormsControlStubBase::IsEventTarget(PRBool *aOK)
{
  *aOK = PR_TRUE;
  return NS_OK;
}
  

nsresult
nsXFormsControlStubBase::ProcessNodeBinding(const nsString          &aBindingAttr,
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

  if (!nsXFormsUtils::IsDocumentReadyForBind(domDoc)) {
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
nsXFormsControlStubBase::ResetHelpAndHint(PRBool aInitialize)
{
  nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(mElement));
  if (!targ)
    return;

  NS_NAMED_LITERAL_STRING(mouseover, "mouseover");
  NS_NAMED_LITERAL_STRING(mouseout, "mouseout");
  NS_NAMED_LITERAL_STRING(focus, "focus");
  NS_NAMED_LITERAL_STRING(blur, "blur");
  NS_NAMED_LITERAL_STRING(keypress, "keypress");

  if (mEventListener) {
    targ->RemoveEventListener(mouseover, mEventListener, PR_TRUE);
    targ->RemoveEventListener(mouseout, mEventListener, PR_TRUE);
    targ->RemoveEventListener(focus, mEventListener, PR_TRUE);
    targ->RemoveEventListener(blur, mEventListener, PR_TRUE);
    targ->RemoveEventListener(keypress, mEventListener, PR_TRUE);
    mEventListener = nsnull;
  }

  if (aInitialize) {
    mEventListener = new nsXFormsHintHelpListener();
    if (!mEventListener)
      return;

    targ->AddEventListener(mouseover, mEventListener, PR_TRUE);
    targ->AddEventListener(mouseout, mEventListener, PR_TRUE);
    targ->AddEventListener(focus, mEventListener, PR_TRUE);
    targ->AddEventListener(blur, mEventListener, PR_TRUE);
    targ->AddEventListener(keypress, mEventListener, PR_TRUE);
  }
}

PRBool
nsXFormsControlStubBase::GetReadOnlyState()
{
  PRBool res = PR_FALSE;
  if (mElement) {
    mElement->HasAttribute(NS_LITERAL_STRING("read-only"), &res);
  }  
  return res;
}

PRBool
nsXFormsControlStubBase::GetRelevantState()
{
  PRBool res = PR_FALSE;
  if (mElement) {
    mElement->HasAttribute(NS_LITERAL_STRING("enabled"), &res);
  }  
  return res;
}

nsresult
nsXFormsControlStubBase::HandleDefault(nsIDOMEvent *aEvent,
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
    } else if (type.Equals(NS_LITERAL_STRING("keypress"))) { 
      nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
      if (keyEvent) {
        PRUint32 keycode;
        keyEvent->GetKeyCode(&keycode);
        if (keycode == nsIDOMKeyEvent::DOM_VK_TAB) {
          PRBool extraKey = PR_FALSE;

          keyEvent->GetAltKey(&extraKey);
          if (extraKey) {
            return NS_OK;
          }

          keyEvent->GetCtrlKey(&extraKey);
          if (extraKey) {
            return NS_OK;
          }

          keyEvent->GetMetaKey(&extraKey);
          if (extraKey) {
            return NS_OK;
          }

          keyEvent->GetShiftKey(&extraKey);
          mPreventLoop = PR_TRUE;
          if (extraKey) {
            nsXFormsUtils::DispatchEvent(mElement, eEvent_Previous);
          } else {
            nsXFormsUtils::DispatchEvent(mElement, eEvent_Next);
          }
        }
      }
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Next].name) ||     
               type.EqualsASCII(sXFormsEventsEntries[eEvent_Previous].name)) { 

      // only continue this processing if xforms-next or xforms-previous were
      // dispatched by the form and not as part of the 'tab' and 'shift+tab'
      // processing
      if (mPreventLoop) {
        mPreventLoop = PR_FALSE;
        return NS_OK;
      }

      nsCOMPtr<nsIDOMDocument> domDoc;
      mElement->GetOwnerDocument(getter_AddRefs(domDoc));

      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
      // element isn't in a document, yet?  Odd, indeed.  Well, if not in
      // document, these two events have no meaning.
      NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);
      nsCOMPtr<nsPIDOMWindow> win = 
                               do_QueryInterface(doc->GetScriptGlobalObject()); 
                                               
    
      // An inelegant way to retrieve this to be sure, but we are
      // guaranteed that the focus controller outlives us, so it
      // is safe to hold on to it (since we can't die until it has
      // died).
      nsIFocusController *focusController = win->GetRootFocusController();
      if (focusController &&
          type.EqualsASCII(sXFormsEventsEntries[eEvent_Next].name)) {
        focusController->MoveFocus(PR_TRUE, nsnull);
      } else {
        focusController->MoveFocus(PR_FALSE, nsnull);
      }
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_BindingException].name)) {
      *aHandled = HandleBindingException();
    }
  }
  
  return NS_OK;
}

PRBool
nsXFormsControlStubBase::HandleBindingException()
{
  if (!mElement) {
    return PR_FALSE;
  }
  nsCOMPtr<nsIDOMDocument> doc;
  mElement->GetOwnerDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIDocument> iDoc(do_QueryInterface(doc));
  if (!iDoc) {
    return PR_FALSE;
  }

  // check for fatalError property, enforcing that only one fatal error will
  // be shown to the user
  if (iDoc->GetProperty(nsXFormsAtoms::fatalError)) {
    return PR_FALSE;
  }
  iDoc->SetProperty(nsXFormsAtoms::fatalError, iDoc);

  // Check for preference, disabling this popup
  PRBool disablePopup = PR_FALSE;
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && pref) {
    PRBool val;
    if (NS_SUCCEEDED(pref->GetBoolPref("xforms.disablePopup", &val)))
      disablePopup = val;
  }
  if (disablePopup)
    return PR_FALSE;

  // Get nsIDOMWindowInternal
  nsCOMPtr<nsIDOMDocumentView> dview(do_QueryInterface(doc));
  if (!dview)
    return PR_FALSE;

  nsCOMPtr<nsIDOMAbstractView> aview;
  dview->GetDefaultView(getter_AddRefs(aview));

  nsCOMPtr<nsIDOMWindowInternal> internal(do_QueryInterface(aview));
  if (!internal)
    return PR_FALSE;

  // Show popup
  nsCOMPtr<nsIDOMWindow> messageWindow;
  rv = internal->OpenDialog(NS_LITERAL_STRING("chrome://xforms/content/bindingex.xul"),
                            NS_LITERAL_STRING("XFormsBindingException"),
                            NS_LITERAL_STRING("modal,dialog,chrome,dependent"),
                            nsnull, getter_AddRefs(messageWindow));
  return NS_SUCCEEDED(rv);
}


#ifdef DEBUG_smaug
static nsVoidArray* sControlList = nsnull;
class ControlDebug
{
public:
  ControlDebug() {
    sControlList = new nsVoidArray();
    NS_ASSERTION(sControlList, "Out of memory!");
  }

  ~ControlDebug() {
    for (PRInt32 i = 0; i < sControlList->Count(); ++i) {
      nsXFormsControlStubBase* control =
        NS_STATIC_CAST(nsXFormsControlStubBase*, sControlList->ElementAt(i));
      if (control) {
        printf("Possible leak, <xforms:%s>\n", control->Name());
      }
    }
    delete sControlList;
    sControlList = nsnull;
  }
};

static ControlDebug tester = ControlDebug();
#endif

nsresult
nsXFormsControlStubBase::Create(nsIXTFElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(kStandardNotificationMask);

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.
  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));
  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  ResetHelpAndHint(PR_TRUE);

  // enabled is on pr. default
  aWrapper->SetIntrinsicState(NS_EVENT_STATE_ENABLED);

#ifdef DEBUG_smaug
  sControlList->AppendElement(this);
#endif

  return NS_OK;
}

nsresult
nsXFormsControlStubBase::OnDestroyed()
{
  ResetHelpAndHint(PR_FALSE);
  RemoveIndexListeners();

  if (mModel) {
    mModel->RemoveFormControl(this);
    mModel = nsnull;
  }

#ifdef DEBUG_smaug
  sControlList->RemoveElement(this);
#endif

  mElement = nsnull;
  return NS_OK;
}

nsresult
nsXFormsControlStubBase::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  // We need to re-evaluate our instance data binding when our document
  // changes, since our context can change
  if (aNewDocument) {
    Bind();
    Refresh();
  }

  return NS_OK;
}

nsresult
nsXFormsControlStubBase::ParentChanged(nsIDOMElement *aNewParent)
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

nsresult
nsXFormsControlStubBase::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  MaybeRemoveFromModel(aName, aValue);
  return NS_OK;
}

nsresult
nsXFormsControlStubBase::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  MaybeBindAndRefresh(aName);
  return NS_OK;
}

nsresult
nsXFormsControlStubBase::WillRemoveAttribute(nsIAtom *aName)
{
  MaybeRemoveFromModel(aName, EmptyString());
  return NS_OK;
}

nsresult
nsXFormsControlStubBase::AttributeRemoved(nsIAtom *aName)
{
  MaybeBindAndRefresh(aName);
  return NS_OK;
}

// nsIXFormsContextControl

NS_IMETHODIMP
nsXFormsControlStubBase::SetContext(nsIDOMNode *aContextNode,
                                    PRInt32     aContextPosition,
                                    PRInt32     aContextSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXFormsControlStubBase::GetContext(nsAString      &aModelID,
                                    nsIDOMNode    **aContextNode,
                                    PRInt32        *aContextPosition,
                                    PRInt32        *aContextSize)
{
  NS_ENSURE_ARG(aContextSize);
  NS_ENSURE_ARG(aContextPosition);

  *aContextPosition = 1;
  *aContextSize = 1;

  if (aContextNode) {
    if (mBoundNode) {
      // We are bound to a node: This is the context node
      CallQueryInterface(mBoundNode, aContextNode); // addrefs
      NS_ASSERTION(*aContextNode, "could not QI context node from bound node?");
    } else if (mModel) {
      // We are bound to a model: The document element of its default instance
      // document is the context node
      nsCOMPtr<nsIDOMDocument> instanceDoc;
      mModel->GetInstanceDocument(EmptyString(), getter_AddRefs(instanceDoc));
      NS_ENSURE_STATE(instanceDoc);

      nsIDOMElement* docElement;
      instanceDoc->GetDocumentElement(&docElement); // addrefs
      NS_ENSURE_STATE(docElement);
      *aContextNode = docElement; // addref'ed above
    }
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
nsXFormsControlStubBase::ResetProperties()
{
  nsCOMPtr<nsIXTFElementWrapper> xtfWrap(do_QueryInterface(mElement));
  if (!xtfWrap) {
    return;
  }

  // enabled is on pr. default
  xtfWrap->SetIntrinsicState(NS_EVENT_STATE_ENABLED);
}

void
nsXFormsControlStubBase::AddRemoveSNBAttr(nsIAtom *aName, const nsAString &aValue) 
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
nsXFormsControlStubBase::MaybeBindAndRefresh(nsIAtom *aName)
{
  if (aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref  ||
      aName == nsXFormsAtoms::model) {

    Bind();
    Refresh();
  }

}

void
nsXFormsControlStubBase::MaybeRemoveFromModel(nsIAtom         *aName, 
                                              const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::model ||
      aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::ref) {
    if (mModel)
      mModel->RemoveFormControl(this);

    AddRemoveSNBAttr(aName, aValue);
  }
}

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsControlStub,
                             nsXFormsXMLVisualStub,
                             nsIXFormsContextControl,
                             nsIXFormsControl,
                             nsIXFormsControlBase)


NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsBindableControlStub,
                             nsXFormsBindableStub,
                             nsIXFormsContextControl,
                             nsIXFormsControl,
                             nsIXFormsControlBase)

