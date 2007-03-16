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
#include "nsIDocument.h"
#include "nsXFormsModelElement.h"
#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"
#include "nsIServiceManager.h"
#include "nsIEventStateManager.h"
#include "nsIContent.h"
#include "nsIDOM3Node.h"
#include "nsIDOMAttr.h"

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
nsXFormsControlStub::ResetBoundNode(const nsString &aBindAttribute,
                                    PRUint16        aResultType,
                                    PRBool         *aContextChanged)
{
  NS_ENSURE_ARG(aContextChanged);

  // Clear existing bound node, etc.
  *aContextChanged = mBoundNode ? PR_TRUE : PR_FALSE;
  nsCOMPtr<nsIDOMNode> oldBoundNode;
  oldBoundNode.swap(mBoundNode);
  mUsesModelBinding = PR_FALSE;
  mAppearDisabled = PR_FALSE;
  mDependencies.Clear();
  RemoveIndexListeners();

  if (!mHasParent || !HasBindingAttribute())
    return NS_OK_XFORMS_NOTREADY;

  nsCOMPtr<nsIDOMXPathResult> result;
  nsresult rv = ProcessNodeBinding(aBindAttribute, aResultType,
                                   getter_AddRefs(result));

  if (NS_FAILED(rv)) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("controlBindError"), mElement);
    return rv;
  }

  if (rv == NS_OK_XFORMS_DEFERRED || !result) {
    // Binding was deferred, or not bound
    return rv;
  }

  // Get context node, if any
  if (mUsesModelBinding) {
    // When bound via @bind, we'll get a snapshot back
    result->SnapshotItem(0, getter_AddRefs(mBoundNode));
  } else {
    result->GetSingleNodeValue(getter_AddRefs(mBoundNode));
  }

  *aContextChanged = (oldBoundNode != mBoundNode);

  // Some controls may not be bound to certain types of content. If the content
  // is a disallowed type, report the error and dispatch a binding exception
  // event.
  PRBool isAllowed = IsContentAllowed();

  if (!mBoundNode || !isAllowed) {
    // If there's no result (ie, no instance node) returned by the above, it
    // means that the binding is not pointing to an instance data node, so we
    // should disable the control.
    mAppearDisabled = PR_TRUE;

    if (!isAllowed) {
      // build the error string that we want output to the ErrorConsole
      nsAutoString localName;
      mElement->GetLocalName(localName);
      const PRUnichar *strings[] = { localName.get() };

      nsXFormsUtils::ReportError(
        NS_LITERAL_STRING("boundTypeErrorComplexContent"),
        strings, 1, mElement, mElement);

      nsXFormsUtils::DispatchEvent(mElement, eEvent_BindingException);
    }

    nsCOMPtr<nsIXTFElementWrapper> wrapper(do_QueryInterface(mElement));
    NS_ENSURE_STATE(wrapper);

    PRInt32 iState;
    GetDisabledIntrinsicState(&iState);
    return wrapper->SetIntrinsicState(iState);
  }

  // Check for presence of @xsi:type on bound node and add as a dependency
  nsCOMPtr<nsIDOMElement> boundEl(do_QueryInterface(mBoundNode));
  if (boundEl) {
    nsCOMPtr<nsIDOMAttr> attrNode;
    rv = boundEl->GetAttributeNodeNS(NS_LITERAL_STRING(NS_NAMESPACE_XML_SCHEMA_INSTANCE),
                                     NS_LITERAL_STRING("type"),
                                     getter_AddRefs(attrNode));
    if (NS_SUCCEEDED(rv) && attrNode) {
      mDependencies.AppendObject(attrNode);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::Bind(PRBool* aContextChanged)
{
  return ResetBoundNode(NS_LITERAL_STRING("ref"),
                        nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                        aContextChanged);
}

NS_IMETHODIMP
nsXFormsControlStub::Refresh()
{
  // XXX: In theory refresh should never be called when there is no model,
  // but that's definately not the case now.
  return (mModel && !mAppearDisabled) ? mModel->SetStates(this, mBoundNode)
                                      : NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}
  
NS_IMETHODIMP
nsXFormsControlStub::IsEventTarget(PRBool *aOK)
{
  *aOK = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::GetUsesModelBinding(PRBool *aRes)
{
  *aRes = mUsesModelBinding;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::GetUsesSingleNodeBinding(PRBool *aRes)
{
  *aRes = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::GetOnDeferredBindList(PRBool *aOnList)
{
  NS_ENSURE_ARG_POINTER(aOnList);
  *aOnList = mOnDeferredBindList;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::SetOnDeferredBindList(PRBool aPutOnList)
{
  mOnDeferredBindList = aPutOnList;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::GetDefaultIntrinsicState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = kDefaultIntrinsicState;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::GetDisabledIntrinsicState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = kDisabledIntrinsicState;
  return NS_OK;
}

nsresult
nsXFormsControlStub::MaybeAddToModel(nsIModelElementPrivate *aOldModel,
                                     nsIXFormsControl       *aParent)
{
  if (GetRepeatState() == eType_Template) {
    // No sense going any further.  A template control has no need to bind,
    // refresh, etc. so no reason to put it on the model's control list
    return NS_OK;
  }

  // XXX: just doing pointer comparison would be nice....
  PRBool sameModel = PR_FALSE;
  nsresult rv;

  if (mModel) {
    nsCOMPtr<nsIDOM3Node> n3Model(do_QueryInterface(mModel));
    nsCOMPtr<nsIDOMNode> nOldModel(do_QueryInterface(aOldModel));
    NS_ASSERTION(n3Model, "model element not supporting nsIDOM3Node?!");
    rv = n3Model->IsSameNode(nOldModel, &sameModel);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    sameModel = !aOldModel;
  }

  if (!sameModel) {
    if (aOldModel) {
      rv = aOldModel->RemoveFormControl(this);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (mModel) {
      rv = mModel->AddFormControl(this, aParent);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


nsresult
nsXFormsControlStub::ProcessNodeBinding(const nsString          &aBindingAttr,
                                        PRUint16                 aResultType,
                                        nsIDOMXPathResult      **aResult,
                                        nsIModelElementPrivate **aModel)
{
  nsStringArray indexesUsed;

  if (aResult) {
    *aResult = nsnull;
  }

  if (aModel) {
    *aModel = nsnull;
  }

  // let's not go through all of this rigamarol if we don't have a chance
  // in heck of binding anyhow.  Check to see if the models will be receptive
  // to some binding.  readyForBindProperty is set when they are.  Make sure
  // to return NS_OK so that we don't start complaining about binding
  // failures in this situation.

  if (!nsXFormsUtils::IsDocumentReadyForBind(mElement)) {
    nsXFormsModelElement::DeferElementBind(this);
    return NS_OK_XFORMS_DEFERRED;
  }

  nsresult rv;
  PRBool usesModelBinding;
  nsCOMPtr<nsIModelElementPrivate> oldModel(mModel);
  nsCOMPtr<nsIXFormsControl> parentControl;
  rv = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                          kElementFlags,
                                          aBindingAttr,
                                          EmptyString(),
                                          aResultType,
                                          getter_AddRefs(mModel),
                                          aResult,
                                          &usesModelBinding,
                                          getter_AddRefs(parentControl),
                                          &mDependencies,
                                          &indexesUsed);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(mModel);

  rv = MaybeAddToModel(oldModel, parentControl);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aModel)
    NS_ADDREF(*aModel = mModel);

  mUsesModelBinding = usesModelBinding;

  if (indexesUsed.Count()) {
    // add index listeners on repeat elements

    for (PRInt32 i = 0; i < indexesUsed.Count(); ++i) {
      // Find the repeat element and add |this| as a listener
      nsCOMPtr<nsIDOMElement> repElem;
      nsXFormsUtils::GetElementByContextId(mElement, *(indexesUsed[i]),
                                           getter_AddRefs(repElem));
      nsCOMPtr<nsIXFormsRepeatElement> rep(do_QueryInterface(repElem));
      if (!rep)
        continue;

      rv = rep->AddIndexUser(this);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mIndexesUsed.AppendObject(rep);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::BindToModel(PRBool aSetBoundNode)
{
  if (GetRepeatState() == eType_Template) {
    // No sense going any further.  A template control has no need to bind,
    // refresh, etc. so no reason to put it on the model's control list
    return NS_OK;
  }

  nsCOMPtr<nsIModelElementPrivate> oldModel(mModel);

  nsCOMPtr<nsIXFormsControl> parentControl;
  nsCOMPtr<nsIDOMNode> boundNode;
  mModel = nsXFormsUtils::GetModel(mElement, getter_AddRefs(parentControl),
                                   kElementFlags,
                                   getter_AddRefs(boundNode));
  if (aSetBoundNode) {
    mBoundNode.swap(boundNode);
  }

  return MaybeAddToModel(oldModel, parentControl);
}

void
nsXFormsControlStub::ResetHelpAndHint(PRBool aInitialize)
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
nsXFormsControlStub::GetRelevantState()
{
  PRBool res = PR_FALSE;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
  if (content && (content->IntrinsicState() & NS_EVENT_STATE_ENABLED)) {
    res = PR_TRUE;
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
    
      // An inelegant way to retrieve this to be sure, but we are
      // guaranteed that the focus controller outlives us, so it
      // is safe to hold on to it (since we can't die until it has
      // died).
      nsIFocusController *focusController =
        doc->GetWindow()->GetRootFocusController();
      if (focusController &&
          type.EqualsASCII(sXFormsEventsEntries[eEvent_Next].name)) {
        focusController->MoveFocus(PR_TRUE, nsnull);
      } else {
        focusController->MoveFocus(PR_FALSE, nsnull);
      }
    } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_BindingException].name)) {
      // we threw up a popup during the nsXFormsUtils::DispatchEvent that sent
      // this error to this control
      *aHandled = PR_TRUE;
    }
  }
  
  return NS_OK;
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
      nsXFormsControlStub* control =
        NS_STATIC_CAST(nsXFormsControlStub*, sControlList->ElementAt(i));
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

NS_IMETHODIMP
nsXFormsControlStub::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  nsXFormsStubElement::OnCreated(aWrapper);
  aWrapper->SetNotificationMask(kStandardNotificationMask);

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.
  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));
  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

#ifdef DEBUG_smaug
  sControlList->AppendElement(this);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::OnDestroyed()
{
  RemoveIndexListeners();
  mDependencies.Clear();

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
nsXFormsControlStub::ForceModelDetach(PRBool aRebind)
{
  if (mModel) {
    // Remove from model, so Bind() will be forced to reattach
    mModel->RemoveFormControl(this);
    mModel = nsnull;
  }

  if (!aRebind) {
    return NS_OK;
  }

  PRBool dummy;
  nsresult rv = Bind(&dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv == NS_OK_XFORMS_DEFERRED ? NS_OK : Refresh();
}

NS_IMETHODIMP
nsXFormsControlStub::WillChangeDocument(nsIDOMDocument *aNewDocument)
{
  SetRepeatState(eType_Unknown);
  ResetHelpAndHint(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  if (aNewDocument) {
    ResetHelpAndHint(PR_TRUE);

    // If we are inserted into a document and we have no model, we are probably
    // being initialized, so we should set our intrinsic state to the default
    // value
    if (!mModel && mElement) {
      nsCOMPtr<nsIXTFElementWrapper> xtfWrap(do_QueryInterface(mElement));
      NS_ENSURE_STATE(xtfWrap);
      PRInt32 iState;
      GetDefaultIntrinsicState(&iState);
      xtfWrap->SetIntrinsicState(iState);
    }
  }

  return ForceModelDetach(mHasParent && aNewDocument);
}

NS_IMETHODIMP
nsXFormsControlStub::WillChangeParent(nsIDOMElement *aNewParent)
{
  SetRepeatState(eType_Unknown);
  return NS_OK;
}


NS_IMETHODIMP
nsXFormsControlStub::ParentChanged(nsIDOMElement *aNewParent)
{
  mHasParent = aNewParent != nsnull;

  UpdateRepeatState(aNewParent);

  // We need to re-evaluate our instance data binding when our parent changes,
  // since xmlns declarations or our context could have changed.
  return ForceModelDetach(mHasParent);
}

NS_IMETHODIMP
nsXFormsControlStub::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  BeforeSetAttribute(aName, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  AfterSetAttribute(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::WillRemoveAttribute(nsIAtom *aName)
{
  BeforeSetAttribute(aName, EmptyString());
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsControlStub::AttributeRemoved(nsIAtom *aName)
{
  AfterSetAttribute(aName);
  return NS_OK;
}

// nsIXFormsContextControl

NS_IMETHODIMP
nsXFormsControlStub::SetContext(nsIDOMNode *aContextNode,
                                PRInt32     aContextPosition,
                                PRInt32     aContextSize)
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

  if (aContextNode) {
    if (mBoundNode) {
      // We are bound to a node: This is the context node
      NS_ADDREF(*aContextNode = mBoundNode);
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
  }
}

PRBool
nsXFormsControlStub::IsBindingAttribute(const nsIAtom *aAttr) const
{
  if (aAttr == nsXFormsAtoms::bind ||
      aAttr == nsXFormsAtoms::ref  ||
      aAttr == nsXFormsAtoms::model) {
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

void
nsXFormsControlStub::AfterSetAttribute(nsIAtom *aName)
{
  if (IsBindingAttribute(aName)) {
    PRBool dummy;
    nsresult rv = Bind(&dummy);
    if (NS_SUCCEEDED(rv) &&
        rv != NS_OK_XFORMS_DEFERRED && rv != NS_OK_XFORMS_NOTREADY)
      Refresh();
  }

}

void
nsXFormsControlStub::BeforeSetAttribute(nsIAtom         *aName,
                                        const nsAString &aValue)
{
  if (IsBindingAttribute(aName)) {
    AddRemoveSNBAttr(aName, aValue);
  }
}

nsresult
nsXFormsControlStub::GetBoundBuiltinType(PRUint16 *aBuiltinType)
{
  NS_ENSURE_ARG_POINTER(aBuiltinType);
  *aBuiltinType = 0;

  NS_ENSURE_STATE(mModel);

  // get type of bound instance data
  nsCOMPtr<nsISchemaType> schemaType;
  nsresult rv = mModel->GetTypeForControl(this, getter_AddRefs(schemaType));
  NS_ENSURE_SUCCESS(rv, rv);

  return mModel->GetRootBuiltinType(schemaType, aBuiltinType);
}

PRBool
nsXFormsControlStub::IsContentAllowed()
{
  return PR_TRUE;
}

PRBool
nsXFormsControlStub::IsContentComplex()
{
  PRBool isComplex = PR_FALSE;

  if (mBoundNode) {
    // If the bound node has any Element children, then it has complexContent.
    nsCOMPtr<nsIDOMNode> currentNode;
    mBoundNode->GetFirstChild(getter_AddRefs(currentNode));
    while (currentNode) {
      PRUint16 nodeType;
      currentNode->GetNodeType(&nodeType);
      if (nodeType == nsIDOMNode::ELEMENT_NODE) {
        isComplex = PR_TRUE;
        break;
      }

      nsCOMPtr<nsIDOMNode> node;
      currentNode->GetNextSibling(getter_AddRefs(node));
      currentNode.swap(node);
    }
  }
  return isComplex;
}

nsRepeatState
nsXFormsControlStub::GetRepeatState()
{
  return mRepeatState;
}

void
nsXFormsControlStub::SetRepeatState(nsRepeatState aState)
{
  mRepeatState = aState;
  return;
}

nsRepeatState
nsXFormsControlStub::UpdateRepeatState(nsIDOMNode *aParent)
{
  nsRepeatState repeatState = eType_NotApplicable;

  nsCOMPtr<nsIDOMNode> parent = aParent;
  while (parent) {
    if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("contextcontainer"))) {
      repeatState = eType_GeneratedContent;
      break;
    }
    if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("repeat"))) {
      repeatState = eType_Template;
      break;
    }
    if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("itemset"))) {
      repeatState = eType_Template;
      break;
    }
    nsCOMPtr<nsIDOMNode> tmp;
    parent->GetParentNode(getter_AddRefs(tmp));
    parent = tmp;
  }

  SetRepeatState(repeatState);
  return repeatState;
}

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsControlStub,
                             nsXFormsStubElement,
                             nsIXFormsContextControl,
                             nsIXFormsControl,
                             nsIXFormsControlBase)

