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
#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIFormSubmission.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMNSHTMLFormControlList.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsEventStateManager.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsInterfaceHashtable.h"
#include "nsContentList.h"
#include "nsGUIEvent.h"
#include "nsSupportsArray.h"

// form submission
#include "nsIFormSubmitObserver.h"
#include "nsIURI.h"
#include "nsIObserverService.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsRange.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

// radio buttons
#include "nsIDOMHTMLInputElement.h"
#include "nsIRadioControlElement.h"
#include "nsIRadioVisitor.h"
#include "nsIRadioGroupContainer.h"

static const int NS_FORM_CONTROL_LIST_HASHTABLE_SIZE = 16;

class nsFormControlList;

// nsHTMLFormElement

class nsHTMLFormElement : public nsGenericHTMLElement,
                          public nsSupportsWeakReference,
                          public nsIDOMHTMLFormElement,
                          public nsIDOMNSHTMLFormElement,
                          public nsIWebProgressListener,
                          public nsIForm,
                          public nsIRadioGroupContainer
{
public:
  nsHTMLFormElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLFormElement();

  nsresult Init();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLFormElement
  NS_DECL_NSIDOMHTMLFORMELEMENT

  // nsIDOMNSHTMLFormElement
  NS_DECL_NSIDOMNSHTMLFORMELEMENT  

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIForm
  NS_IMETHOD AddElement(nsIFormControl* aElement);
  NS_IMETHOD AddElementToTable(nsIFormControl* aChild,
                               const nsAString& aName);
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const;
  NS_IMETHOD GetElementCount(PRUint32* aCount) const;
  NS_IMETHOD RemoveElement(nsIFormControl* aElement);
  NS_IMETHOD RemoveElementFromTable(nsIFormControl* aElement,
                                    const nsAString& aName);
  NS_IMETHOD ResolveName(const nsAString& aName,
                         nsISupports** aReturn);
  NS_IMETHOD IndexOfControl(nsIFormControl* aControl, PRInt32* aIndex);
  NS_IMETHOD GetControlEnumerator(nsISimpleEnumerator** aEnumerator);
  NS_IMETHOD OnSubmitClickBegin();
  NS_IMETHOD OnSubmitClickEnd();
  NS_IMETHOD FlushPendingSubmission();
  NS_IMETHOD ForgetPendingSubmission();
  NS_IMETHOD GetActionURL(nsIURI** aActionURL);

  // nsIRadioGroupContainer
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio);
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio);
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const PRBool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadioOut);
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor);
  NS_IMETHOD AddToRadioGroup(const nsAString& aName,
                             nsIFormControl* aRadio);
  NS_IMETHOD RemoveFromRadioGroup(const nsAString& aName,
                                  nsIFormControl* aRadio);

  // nsIContent
  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

  /**
   * Forget all information about the current submission (and the fact that we
   * are currently submitting at all).
   */
  void ForgetCurrentSubmission();

  /**
   * Compare two nodes in the same tree (Negative result means a < b, 0 ==,
   * positive >).  This function may fail if the nodes are not in a tree
   * or are in different trees.
   *
   * @param a the first node
   * @param b the second node
   * @param retval whether a < b (negative), a == b (0), or a > b (positive)
   */
  static nsresult CompareNodes(nsIDOMNode* a,
                               nsIDOMNode* b,
                               PRInt32* retval);

protected:
  nsresult DoSubmitOrReset(nsPresContext* aPresContext,
                           nsEvent* aEvent,
                           PRInt32 aMessage);
  nsresult DoReset();

  //
  // Submit Helpers
  //
  //
  /**
   * Attempt to submit (submission might be deferred) 
   * (called by DoSubmitOrReset)
   *
   * @param aPresContext the presentation context
   * @param aEvent the DOM event that was passed to us for the submit
   */
  nsresult DoSubmit(nsPresContext* aPresContext, nsEvent* aEvent);

  /**
   * Prepare the submission object (called by DoSubmit)
   *
   * @param aPresContext the presentation context
   * @param aFormSubmission the submission object
   * @param aEvent the DOM event that was passed to us for the submit
   */
  nsresult BuildSubmission(nsPresContext* aPresContext, 
                           nsCOMPtr<nsIFormSubmission>& aFormSubmission, 
                           nsEvent* aEvent);
  /**
   * Perform the submission (called by DoSubmit and FlushPendingSubmission)
   *
   * @param aPresContext the presentation context
   * @param aFormSubmission the submission object
   */
  nsresult SubmitSubmission(nsPresContext* aPresContext, 
                            nsIFormSubmission* aFormSubmission);
  /**
   * Walk over the form elements and call SubmitNamesValues() on them to get
   * their data pumped into the FormSubmitter.
   *
   * @param aFormSubmission the form submission object
   * @param aSubmitElement the element that was clicked on (nsnull if none)
   */
  nsresult WalkFormElements(nsIFormSubmission* aFormSubmission,
                            nsIContent* aSubmitElement);

  /**
   * Notify any submit observsers of the submit.
   *
   * @param aActionURL the URL being submitted to
   * @param aCancelSubmit out param where submit observers can specify that the
   *        submit should be cancelled.
   */
  nsresult NotifySubmitObservers(nsIURI* aActionURL, PRBool* aCancelSubmit);

  //
  // Data members
  //
  /** The list of controls (form.elements as well as stuff not in elements) */
  nsFormControlList *mControls;
  /** The currently selected radio button of each group */
  nsInterfaceHashtable<nsStringHashKey,nsIDOMHTMLInputElement> mSelectedRadioButtons;
  /** Whether we are currently processing a submit event or not */
  PRPackedBool mGeneratingSubmit;
  /** Whether we are currently processing a reset event or not */
  PRPackedBool mGeneratingReset;
  /** Whether we are submitting currently */
  PRPackedBool mIsSubmitting;
  /** Whether the submission is to be deferred in case a script triggers it */
  PRPackedBool mDeferSubmission;
  /** Keep track of what the popup state was when the submit was initiated */
  PopupControlState mSubmitPopupState;
  /** Keep track of whether a submission was user-initiated or not */
  PRBool mSubmitInitiatedFromUserInput;

  /** The pending submission object */
  nsCOMPtr<nsIFormSubmission> mPendingSubmission;
  /** The request currently being submitted */
  nsCOMPtr<nsIRequest> mSubmittingRequest;
  /** The web progress object we are currently listening to */
  nsCOMPtr<nsIWebProgress> mWebProgress;

  friend class nsFormControlEnumerator;

protected:
  /** Detection of first form to notify observers */
  static PRBool gFirstFormSubmitted;
  /** Detection of first password input to initialize the password manager */
  static PRBool gPasswordManagerInitialized;
};

PRBool nsHTMLFormElement::gFirstFormSubmitted = PR_FALSE;
PRBool nsHTMLFormElement::gPasswordManagerInitialized = PR_FALSE;


// nsFormControlList
class nsFormControlList : public nsIDOMNSHTMLFormControlList,
                          public nsIDOMHTMLCollection
{
public:
  nsFormControlList(nsIDOMHTMLFormElement* aForm);
  virtual ~nsFormControlList();

  nsresult Init();

  void Clear();
  void SetForm(nsIDOMHTMLFormElement* aForm);

  NS_DECL_ISUPPORTS

  // nsIDOMHTMLCollection interface
  NS_DECL_NSIDOMHTMLCOLLECTION

  // nsIDOMNSHTMLFormControlList interface
  NS_DECL_NSIDOMNSHTMLFORMCONTROLLIST

  nsresult GetNamedObject(const nsAString& aName,
                          nsISupports **aResult);

  nsresult AddElementToTable(nsIFormControl* aChild,
                             const nsAString& aName);
  nsresult RemoveElementFromTable(nsIFormControl* aChild,
                                  const nsAString& aName);
  nsresult IndexOfControl(nsIFormControl* aControl,
                          PRInt32* aIndex);

  nsIDOMHTMLFormElement* mForm;  // WEAK - the form owns me

  nsAutoVoidArray mElements;  // Holds WEAK references - bug 36639

  // This array holds on to all form controls that are not contained
  // in mElements (form.elements in JS, see ShouldBeInFormControl()).
  // This is needed to properly clean up the bi-directional references
  // (both weak and strong) between the form and its form controls.

  nsSmallVoidArray mNotInElements; // Holds WEAK references

protected:
  // A map from an ID or NAME attribute to the form control(s), this
  // hash holds strong references either to the named form control, or
  // to a list of named form controls, in the case where this hash
  // holds on to a list of named form controls the list has weak
  // references to the form control.

  nsInterfaceHashtable<nsStringHashKey,nsISupports> mNameLookupTable;
};


class nsFormControlEnumerator : public nsISimpleEnumerator {
public:
  nsFormControlEnumerator(nsHTMLFormElement* aForm);
  virtual ~nsFormControlEnumerator() { };

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

private:
  nsHTMLFormElement* mForm;
  PRUint32 mElementsIndex;
  nsSupportsArray mNotInElementsSorted;
  PRUint32 mNotInElementsIndex;
};


static PRBool
ShouldBeInElements(nsIFormControl* aFormControl)
{
  // For backwards compatibility (with 4.x and IE) we must not add
  // <input type=image> elements to the list of form controls in a
  // form.

  switch (aFormControl->GetType()) {
  case NS_FORM_BUTTON_BUTTON :
  case NS_FORM_BUTTON_RESET :
  case NS_FORM_BUTTON_SUBMIT :
  case NS_FORM_INPUT_BUTTON :
  case NS_FORM_INPUT_CHECKBOX :
  case NS_FORM_INPUT_FILE :
  case NS_FORM_INPUT_HIDDEN :
  case NS_FORM_INPUT_RESET :
  case NS_FORM_INPUT_PASSWORD :
  case NS_FORM_INPUT_RADIO :
  case NS_FORM_INPUT_SUBMIT :
  case NS_FORM_INPUT_TEXT :
  case NS_FORM_SELECT :
  case NS_FORM_TEXTAREA :
  case NS_FORM_FIELDSET :
  case NS_FORM_OBJECT :
    return PR_TRUE;
  }

  // These form control types are not supposed to end up in the
  // form.elements array
  //
  // NS_FORM_INPUT_IMAGE
  // NS_FORM_LABEL
  // NS_FORM_OPTION
  // NS_FORM_OPTGROUP
  // NS_FORM_LEGEND

  return PR_FALSE;
}

// nsHTMLFormElement implementation

// construction, destruction
nsIHTMLContent*
NS_NewHTMLFormElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  nsHTMLFormElement* it = new nsHTMLFormElement(aNodeInfo);
  if (!it) {
    return nsnull;
  }

  nsresult rv = it->Init();

  if (NS_FAILED(rv)) {
    delete it;
    return nsnull;
  }

  return it;
}

nsHTMLFormElement::nsHTMLFormElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mGeneratingSubmit(PR_FALSE),
    mGeneratingReset(PR_FALSE),
    mIsSubmitting(PR_FALSE),
    mDeferSubmission(PR_FALSE),
    mSubmitPopupState(openAbused),
    mSubmitInitiatedFromUserInput(PR_FALSE),
    mPendingSubmission(nsnull),
    mSubmittingRequest(nsnull)
{
}

nsHTMLFormElement::~nsHTMLFormElement()
{
  if (mControls) {
    mControls->Clear();
    mControls->SetForm(nsnull);

    NS_RELEASE(mControls);
  }
}

nsresult
nsHTMLFormElement::Init()
{
  mControls = new nsFormControlList(this);
  if (!mControls) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = mControls->Init();
  
  if (NS_FAILED(rv))
  {
    delete mControls;
    mControls = nsnull;
    return rv;
  }
  
  NS_ADDREF(mControls);

  NS_ENSURE_TRUE(mSelectedRadioButtons.Init(4),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}


// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLFormElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLFormElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLFormElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFormElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIForm)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIRadioGroupContainer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFormElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLFormElement

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsHTMLFormElement)

NS_IMETHODIMP
nsHTMLFormElement::GetElements(nsIDOMHTMLCollection** aElements)
{
  *aElements = mControls;
  NS_ADDREF(mControls);
  return NS_OK;
}

nsresult
nsHTMLFormElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
{
  if ((aName == nsHTMLAtoms::action || aName == nsHTMLAtoms::target) &&
      aNameSpaceID == kNameSpaceID_None) {
    if (mPendingSubmission) {
      // aha, there is a pending submission that means we're in
      // the script and we need to flush it. let's tell it
      // that the event was ignored to force the flush.
      // the second argument is not playing a role at all.
      FlushPendingSubmission();
    }
    ForgetCurrentSubmission();
  }
  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                       aNotify);
}

NS_IMPL_STRING_ATTR(nsHTMLFormElement, AcceptCharset, acceptcharset)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Action, action)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Enctype, enctype)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Method, method)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Name, name)

NS_IMETHODIMP
nsHTMLFormElement::GetTarget(nsAString& aValue)
{
  aValue.Truncate();
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, aValue);
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    GetBaseTarget(aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::SetTarget(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::target, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLFormElement::Submit()
{
  // Send the submit event
  nsresult rv = NS_OK;
  nsCOMPtr<nsPresContext> presContext = GetPresContext();
  if (presContext) {
    if (mPendingSubmission) {
      // aha, we have a pending submission that was not flushed
      // (this happens when form.submit() is called twice)
      // we have to delete it and build a new one since values
      // might have changed inbetween (we emulate IE here, that's all)
      mPendingSubmission = nsnull;
    }

    rv = DoSubmitOrReset(presContext, nsnull, NS_FORM_SUBMIT);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLFormElement::Reset()
{
  // Send the reset event
  nsresult rv = NS_OK;
  nsCOMPtr<nsPresContext> presContext = GetPresContext();
  if (presContext) {
    // Calling HandleDOMEvent() directly so that reset() will work even if
    // the frame does not exist.  This does not have an effect right now, but
    // If PresShell::HandleEventWithTarget() ever starts to work for elements
    // without frames, that should be called instead.
    nsFormEvent event(NS_FORM_RESET);
    nsEventStatus status  = nsEventStatus_eIgnore;
    HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }
  return rv;
}

static const nsHTMLValue::EnumTable kFormMethodTable[] = {
  { "get", NS_FORM_METHOD_GET },
  { "post", NS_FORM_METHOD_POST },
  { 0 }
};

static const nsHTMLValue::EnumTable kFormEnctypeTable[] = {
  { "multipart/form-data", NS_FORM_ENCTYPE_MULTIPART },
  { "application/x-www-form-urlencoded", NS_FORM_ENCTYPE_URLENCODED },
  { "text/plain", NS_FORM_ENCTYPE_TEXTPLAIN },
  { 0 }
};

PRBool
nsHTMLFormElement::ParseAttribute(nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::method) {
    return aResult.ParseEnumValue(aValue, kFormMethodTable);
  }
  if (aAttribute == nsHTMLAtoms::enctype) {
    return aResult.ParseEnumValue(aValue, kFormEnctypeTable);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLFormElement::AttributeToString(nsIAtom* aAttribute,
                                     const nsHTMLValue& aValue,
                                     nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::method) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      aValue.EnumValueToString(kFormMethodTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::enctype) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      aValue.EnumValueToString(kFormEnctypeTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

void
nsHTMLFormElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                               PRBool aCompileEventHandlers)
{
  nsCOMPtr<nsIHTMLDocument> oldDocument = do_QueryInterface(GetCurrentDoc());
  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);
  
  nsCOMPtr<nsIHTMLDocument> newDocument = do_QueryInterface(GetCurrentDoc());
  if (oldDocument != newDocument) {
    if (oldDocument) {
      oldDocument->RemovedForm();
      ForgetCurrentSubmission();
    }
    if (newDocument) {
      newDocument->AddedForm();
    }
  }
}


nsresult
nsHTMLFormElement::HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent,
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  // If this is the bubble stage, there is a nested form below us which received
  // a submit event.  We do *not* want to handle the submit event for this form
  // too.  So to avert a disaster, we stop the bubbling altogether.
  if ((aFlags & NS_EVENT_FLAG_BUBBLE) &&
      (aEvent->message == NS_FORM_RESET || aEvent->message == NS_FORM_SUBMIT)) {
    return NS_OK;
  }

  // Ignore recursive calls to submit and reset
  if (aEvent->message == NS_FORM_SUBMIT) {
    if (mGeneratingSubmit) {
      return NS_OK;
    }
    mGeneratingSubmit = PR_TRUE;

    // let the form know that it needs to defer the submission,
    // that means that if there are scripted submissions, the
    // latest one will be deferred until after the exit point of the handler. 
    mDeferSubmission = PR_TRUE;
  }
  else if (aEvent->message == NS_FORM_RESET) {
    if (mGeneratingReset) {
      return NS_OK;
    }
    mGeneratingReset = PR_TRUE;
  }


  nsresult rv = nsGenericHTMLElement::HandleDOMEvent(aPresContext, aEvent,
                                                     aDOMEvent, aFlags,
                                                     aEventStatus); 
  if (aEvent->message == NS_FORM_SUBMIT) {
    // let the form know not to defer subsequent submissions
    mDeferSubmission = PR_FALSE;
  }

  if (NS_SUCCEEDED(rv) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE) &&
      !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {

    if (*aEventStatus == nsEventStatus_eIgnore) {
      switch (aEvent->message) {
        case NS_FORM_RESET:
        case NS_FORM_SUBMIT:
        {
          if (mPendingSubmission && aEvent->message == NS_FORM_SUBMIT) {
            // tell the form to forget a possible pending submission.
            // the reason is that the script returned true (the event was
            // ignored) so if there is a stored submission, it will miss
            // the name/value of the submitting element, thus we need
            // to forget it and the form element will build a new one
            ForgetPendingSubmission();
          }
          DoSubmitOrReset(aPresContext, aEvent, aEvent->message);
        }
        break;
      }
    } else {
      if (aEvent->message == NS_FORM_SUBMIT) {
        // tell the form to flush a possible pending submission.
        // the reason is that the script returned false (the event was
        // not ignored) so if there is a stored submission, it needs to
        // be submitted immediatelly.
        FlushPendingSubmission();
      }
    }
  }

  if (aEvent->message == NS_FORM_SUBMIT) {
    mGeneratingSubmit = PR_FALSE;
  }
  else if (aEvent->message == NS_FORM_RESET) {
    mGeneratingReset = PR_FALSE;
  }

  return rv;
}

nsresult
nsHTMLFormElement::DoSubmitOrReset(nsPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   PRInt32 aMessage)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  // Make sure the presentation is up-to-date
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    doc->FlushPendingNotifications(Flush_ContentAndNotify);
  }

  // JBK Don't get form frames anymore - bug 34297

  // Submit or Reset the form
  nsresult rv = NS_OK;
  if (NS_FORM_RESET == aMessage) {
    rv = DoReset();
  }
  else if (NS_FORM_SUBMIT == aMessage) {
    rv = DoSubmit(aPresContext, aEvent);
  }
  return rv;
}

nsresult
nsHTMLFormElement::DoReset()
{
  // JBK walk the elements[] array instead of form frame controls - bug 34297
  PRUint32 numElements;
  GetElementCount(&numElements);
  for (PRUint32 elementX = 0; (elementX < numElements); elementX++) {
    nsCOMPtr<nsIFormControl> controlNode;
    GetElementAt(elementX, getter_AddRefs(controlNode));
    if (controlNode) {
      controlNode->Reset();
    }
  }

  return NS_OK;
}

#define NS_ENSURE_SUBMIT_SUCCESS(rv)                                          \
  if (NS_FAILED(rv)) {                                                        \
    ForgetCurrentSubmission();                                                \
    return rv;                                                                \
  }

nsresult
nsHTMLFormElement::DoSubmit(nsPresContext* aPresContext, nsEvent* aEvent)
{
  NS_ASSERTION(!mIsSubmitting, "Either two people are trying to submit or the "
               "previous submit was not properly cancelled by the DocShell");
  if (mIsSubmitting) {
    // XXX Should this return an error?
    return NS_OK;
  }

  // Mark us as submitting so that we don't try to submit again
  mIsSubmitting = PR_TRUE;
  NS_ASSERTION(!mWebProgress && !mSubmittingRequest, "Web progress / submitting request should not exist here!");

  nsCOMPtr<nsIFormSubmission> submission;
   
  //
  // prepare the submission object
  //
  BuildSubmission(aPresContext, submission, aEvent); 

  // XXXbz if the script global is that for an sXBL/XBL2 doc, it won't
  // be a window...
  nsCOMPtr<nsPIDOMWindow> window =
    do_QueryInterface(GetOwnerDoc()->GetScriptGlobalObject());

  if (window) {
    mSubmitPopupState = window->GetPopupControlState();
  } else {
    mSubmitPopupState = openAbused;
  }

  mSubmitInitiatedFromUserInput = nsEventStateManager::IsHandlingUserInput();

  if(mDeferSubmission) { 
    // we are in an event handler, JS submitted so we have to
    // defer this submission. let's remember it and return
    // without submitting
    mPendingSubmission = submission;
    // ensure reentrancy
    mIsSubmitting = PR_FALSE;
    return NS_OK; 
  } 
  
  // 
  // perform the submission
  //
  return SubmitSubmission(aPresContext, submission); 
}

nsresult
nsHTMLFormElement::BuildSubmission(nsPresContext* aPresContext, 
                                   nsCOMPtr<nsIFormSubmission>& aFormSubmission, 
                                   nsEvent* aEvent)
{
  NS_ASSERTION(!mPendingSubmission, "tried to build two submissions!");

  // Get the originating frame (failure is non-fatal)
  nsIContent *originatingElement = nsnull;
  if (aEvent) {
    if (NS_FORM_EVENT == aEvent->eventStructType) {
      originatingElement = ((nsFormEvent *)aEvent)->originator;
    }
  }

  nsresult rv;

  //
  // Get the submission object
  //
  rv = GetSubmissionFromForm(this, aPresContext, getter_AddRefs(aFormSubmission));
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  //
  // Dump the data into the submission object
  //
  rv = WalkFormElements(aFormSubmission, originatingElement);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  return NS_OK;
}

nsresult
nsHTMLFormElement::SubmitSubmission(nsPresContext* aPresContext, 
                                    nsIFormSubmission* aFormSubmission)
{
  nsresult rv;
  //
  // Get the action and target
  //
  nsCOMPtr<nsIURI> actionURI;
  rv = GetActionURL(getter_AddRefs(actionURI));
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  if (!actionURI) {
    mIsSubmitting = PR_FALSE;
    return NS_OK;
  }

  // javascript URIs are not really submissions; they just call a function.
  // Also, they may synchronously call submit(), and we want them to be able to
  // do so while still disallowing other double submissions. (Bug 139798)
  // Note that any other URI types that are of equivalent type should also be
  // added here.
  PRBool schemeIsJavaScript = PR_FALSE;
  if (NS_SUCCEEDED(actionURI->SchemeIs("javascript", &schemeIsJavaScript)) &&
      schemeIsJavaScript) {
    mIsSubmitting = PR_FALSE;
  }

  nsAutoString target;
  rv = GetTarget(target);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  //
  // Notify observers of submit
  //
  PRBool aCancelSubmit = PR_FALSE;
  rv = NotifySubmitObservers(actionURI, &aCancelSubmit);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  if (aCancelSubmit) {
    mIsSubmitting = PR_FALSE;
    return NS_OK;
  }

  //
  // Submit
  //
  nsCOMPtr<nsIDocShell> docShell;

  {
    nsAutoPopupStatePusher popupStatePusher(mSubmitPopupState);

    nsAutoHandlingUserInputStatePusher userInpStatePusher(mSubmitInitiatedFromUserInput);

    rv = aFormSubmission->SubmitTo(actionURI, target, this, aPresContext,
                                   getter_AddRefs(docShell),
                                   getter_AddRefs(mSubmittingRequest));
  }

  NS_ENSURE_SUBMIT_SUCCESS(rv);

  // Even if the submit succeeds, it's possible for there to be no docshell
  // or request; for example, if it's to a named anchor within the same page
  // the submit will not really do anything.
  if (docShell) {
    // If the channel is pending, we have to listen for web progress.
    PRBool pending = PR_FALSE;
    mSubmittingRequest->IsPending(&pending);
    if (pending) {
      mWebProgress = do_GetInterface(docShell);
      NS_ASSERTION(mWebProgress, "nsIDocShell not converted to nsIWebProgress!");
      rv = mWebProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_ALL);
      NS_ENSURE_SUBMIT_SUCCESS(rv);
    } else {
      ForgetCurrentSubmission();
    }
  } else {
    ForgetCurrentSubmission();
  }

  return rv;
}

nsresult
nsHTMLFormElement::NotifySubmitObservers(nsIURI* aActionURL,
                                         PRBool* aCancelSubmit)
{
  // If this is the first form, bring alive the first form submit
  // category observers
  if (!gFirstFormSubmitted) {
    gFirstFormSubmitted = PR_TRUE;
    NS_CreateServicesFromCategory(NS_FIRST_FORMSUBMIT_CATEGORY,
                                  nsnull,
                                  NS_FIRST_FORMSUBMIT_CATEGORY);
  }

  // Notify observers that the form is being submitted.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIObserverService> service =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> theEnum;
  rv = service->EnumerateObservers(NS_FORMSUBMIT_SUBJECT,
                                   getter_AddRefs(theEnum));
  NS_ENSURE_SUCCESS(rv, rv);

  if (theEnum) {
    nsCOMPtr<nsISupports> inst;
    *aCancelSubmit = PR_FALSE;

    // XXXbz what do the submit observers actually want?  The window
    // of the document this is shown in?  Or something else?
    // sXBL/XBL2 issue
    nsCOMPtr<nsIDOMWindowInternal> window =
      do_QueryInterface(GetOwnerDoc()->GetScriptGlobalObject());

    PRBool loop = PR_TRUE;
    while (NS_SUCCEEDED(theEnum->HasMoreElements(&loop)) && loop) {
      theEnum->GetNext(getter_AddRefs(inst));

      nsCOMPtr<nsIFormSubmitObserver> formSubmitObserver(
                      do_QueryInterface(inst));
      if (formSubmitObserver) {
        rv = formSubmitObserver->Notify(this,
                                        window,
                                        aActionURL,
                                        aCancelSubmit);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      if (*aCancelSubmit) {
        return NS_OK;
      }
    }
  }

  return rv;
}


// static
nsresult
nsHTMLFormElement::CompareNodes(nsIDOMNode* a, nsIDOMNode* b, PRInt32* retval)
{
  nsresult rv;

  nsCOMPtr<nsIDOMNode> parentANode;
  PRInt32 indexA;
  rv = a->GetParentNode(getter_AddRefs(parentANode));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!parentANode) {
    return NS_ERROR_UNEXPECTED;
  }

  {
    // To get the index, we must turn them both into contents
    // and do IndexOf().  Ick.
    nsCOMPtr<nsIContent> parentA(do_QueryInterface(parentANode));
    nsCOMPtr<nsIContent> contentA(do_QueryInterface(a));
    if (!parentA || !contentA) {
      return NS_ERROR_UNEXPECTED;
    }
    indexA = parentA->IndexOf(contentA);
  }

  nsCOMPtr<nsIDOMNode> parentBNode;
  PRInt32 indexB;
  rv = b->GetParentNode(getter_AddRefs(parentBNode));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!parentBNode) {
    return NS_ERROR_UNEXPECTED;
  }

  {
    // To get the index, we must turn them both into contents
    // and do IndexOf().  Ick.
    nsCOMPtr<nsIContent> parentB(do_QueryInterface(parentBNode));
    nsCOMPtr<nsIContent> bContent(do_QueryInterface(b));
    if (!parentB || !bContent) {
      return NS_ERROR_UNEXPECTED;
    }
    indexB = parentB->IndexOf(bContent);
  }

  *retval = nsRange::ComparePoints(parentANode, indexA, parentBNode, indexB);
  return NS_OK;
}


nsresult
nsHTMLFormElement::WalkFormElements(nsIFormSubmission* aFormSubmission,
                                    nsIContent* aSubmitElement)
{
  nsCOMPtr<nsISimpleEnumerator> formControls;
  nsresult rv = GetControlEnumerator(getter_AddRefs(formControls));
  NS_ENSURE_SUCCESS(rv, rv);

  //
  // Walk the list of nodes and call SubmitNamesValues() on the controls
  //
  PRBool hasMoreElements;
  nsCOMPtr<nsISupports> controlSupports;
  nsCOMPtr<nsIFormControl> control;
  while (NS_SUCCEEDED(formControls->HasMoreElements(&hasMoreElements)) &&
         hasMoreElements) {
    rv = formControls->GetNext(getter_AddRefs(controlSupports));
    NS_ENSURE_SUCCESS(rv, rv);
    control = do_QueryInterface(controlSupports);

    // Tell the control to submit its name/value pairs to the submission
    control->SubmitNamesValues(aFormSubmission, aSubmitElement);
  }

  return NS_OK;
}

// nsIForm

NS_IMETHODIMP
nsHTMLFormElement::GetElementCount(PRUint32* aCount) const 
{
  mControls->GetLength(aCount); 
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFormElement::GetElementAt(PRInt32 aIndex,
                                nsIFormControl** aFormControl) const 
{
  *aFormControl = NS_STATIC_CAST(nsIFormControl *,
                                 mControls->mElements.SafeElementAt(aIndex));
  NS_IF_ADDREF(*aFormControl);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::AddElement(nsIFormControl* aChild)
{
  NS_ENSURE_TRUE(mControls, NS_ERROR_UNEXPECTED);

  if (ShouldBeInElements(aChild)) {
    // WEAK - don't addref
    mControls->mElements.AppendElement(aChild);
  } else {
    // WEAK - don't addref
    mControls->mNotInElements.AppendElement(aChild);
  }

  //
  // Notify the radio button it's been added to a group
  //
  PRInt32 type = aChild->GetType();
  if (type == NS_FORM_INPUT_RADIO) {
    nsCOMPtr<nsIRadioControlElement> radio = do_QueryInterface(aChild);
    nsresult rv = radio->AddedToRadioGroup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  //
  // If it is a password control, and the password manager has not yet been
  // initialized, initialize the password manager
  //
  if (!gPasswordManagerInitialized && type == NS_FORM_INPUT_PASSWORD) {
    // Initialize the password manager category
    gPasswordManagerInitialized = PR_TRUE;
    NS_CreateServicesFromCategory(NS_PASSWORDMANAGER_CATEGORY,
                                  nsnull,
                                  NS_PASSWORDMANAGER_CATEGORY);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::AddElementToTable(nsIFormControl* aChild,
                                     const nsAString& aName)
{
  NS_ENSURE_TRUE(mControls, NS_ERROR_UNEXPECTED);

  return mControls->AddElementToTable(aChild, aName);  
}


NS_IMETHODIMP 
nsHTMLFormElement::RemoveElement(nsIFormControl* aChild) 
{
  NS_ENSURE_TRUE(mControls, NS_ERROR_UNEXPECTED);

  //
  // Remove it from the radio group if it's a radio button
  //
  if (aChild->GetType() == NS_FORM_INPUT_RADIO) {
    nsCOMPtr<nsIRadioControlElement> radio = do_QueryInterface(aChild);
    nsresult rv = radio->WillRemoveFromRadioGroup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (ShouldBeInElements(aChild)) {
    mControls->mElements.RemoveElement(aChild);
  } else {
    mControls->mNotInElements.RemoveElement(aChild);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::RemoveElementFromTable(nsIFormControl* aElement,
                                          const nsAString& aName)
{
  NS_ENSURE_TRUE(mControls, NS_ERROR_UNEXPECTED);

  return mControls->RemoveElementFromTable(aElement, aName);
}

NS_IMETHODIMP
nsHTMLFormElement::ResolveName(const nsAString& aName,
                               nsISupports **aResult)
{
  return mControls->GetNamedObject(aName, aResult);
}

NS_IMETHODIMP
nsHTMLFormElement::OnSubmitClickBegin()
{
  mDeferSubmission = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::OnSubmitClickEnd()
{
  mDeferSubmission = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::FlushPendingSubmission()
{
  nsCOMPtr<nsIFormSubmission> kunkFuDeathGrip(mPendingSubmission);

  if (!mPendingSubmission) {
    return NS_OK;
  }

  //
  // perform the submission with the stored pending submission
  //
  nsCOMPtr<nsPresContext> presContext = GetPresContext();
  SubmitSubmission(presContext, mPendingSubmission);

  // now delete the pending submission object
  mPendingSubmission = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::ForgetPendingSubmission()
{
  // just delete the pending submission
  mPendingSubmission = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetActionURL(nsIURI** aActionURL)
{
  nsresult rv = NS_OK;

  *aActionURL = nsnull;

  //
  // Grab the URL string
  //
  nsAutoString action;
  GetAction(action);

  //
  // Form the full action URL
  //

  // Get the document to form the URL.
  // We'll also need it later to get the DOM window when notifying form submit
  // observers (bug 33203)
  if (!IsInDoc()) {
    return NS_OK; // No doc means don't submit, see Bug 28988
  }

  // Get base URL
  nsIDocument *document = GetOwnerDoc();
  nsIURI *docURI = document->GetDocumentURI();
  NS_ENSURE_TRUE(docURI, NS_ERROR_UNEXPECTED);

  // If an action is not specified and we are inside
  // a HTML document then reload the URL. This makes us
  // compatible with 4.x browsers.
  // If we are in some other type of document such as XML or
  // XUL, do nothing. This prevents undesirable reloading of
  // a document inside XUL.

  nsCOMPtr<nsIURI> actionURL;
  if (action.IsEmpty()) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(document));
    if (!htmlDoc) {
      // Must be a XML, XUL or other non-HTML document type
      // so do nothing.
      return NS_OK;
    }

    rv = docURI->Clone(getter_AddRefs(actionURL));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsIURI> baseURL = GetBaseURI();
    NS_ASSERTION(baseURL, "No Base URL found in Form Submit!\n");
    if (!baseURL) {
      return NS_OK; // No base URL -> exit early, see Bug 30721
    }
    rv = NS_NewURI(getter_AddRefs(actionURL), action, nsnull, baseURL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  //
  // Verify the URL should be reached
  //
  // Get security manager, check to see if access to action URI is allowed.
  //
  nsIScriptSecurityManager *securityManager =
      nsContentUtils::GetSecurityManager();
  rv = securityManager->
    CheckLoadURIWithPrincipal(document->GetPrincipal(), actionURL,
                              nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS(rv, rv);

  //
  // Assign to the output
  //
  *aActionURL = actionURL;
  NS_ADDREF(*aActionURL);

  return rv;
}

NS_IMETHODIMP
nsHTMLFormElement::GetEncoding(nsAString& aEncoding)
{
  return GetEnctype(aEncoding);
}
 
NS_IMETHODIMP
nsHTMLFormElement::SetEncoding(const nsAString& aEncoding)
{
  return SetEnctype(aEncoding);
}
 
NS_IMETHODIMP    
nsHTMLFormElement::GetLength(PRInt32* aLength)
{
  *aLength = mControls->mElements.Count();
  
  return NS_OK;
}

void
nsHTMLFormElement::ForgetCurrentSubmission()
{
  mIsSubmitting = PR_FALSE;
  mSubmittingRequest = nsnull;
  if (mWebProgress) {
    mWebProgress->RemoveProgressListener(this);
    mWebProgress = nsnull;
  }
}

// nsIWebProgressListener
NS_IMETHODIMP
nsHTMLFormElement::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 PRUint32 aStateFlags,
                                 PRUint32 aStatus)
{
  // If STATE_STOP is never fired for any reason (redirect?  Failed state
  // change?) the form element will leak.  It will be kept around by the
  // nsIWebProgressListener (assuming it keeps a strong pointer).  We will
  // consequently leak the request.
  if (aRequest == mSubmittingRequest &&
      aStateFlags & nsIWebProgressListener::STATE_STOP) {
    ForgetCurrentSubmission();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress,
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI* location)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetControlEnumerator(nsISimpleEnumerator** aEnum)
{
  *aEnum = new nsFormControlEnumerator(this);
  NS_ENSURE_TRUE(*aEnum, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aEnum);
  return NS_OK;
}
 
NS_IMETHODIMP
nsHTMLFormElement::IndexOfControl(nsIFormControl* aControl, PRInt32* aIndex)
{
  NS_ENSURE_TRUE(mControls, NS_ERROR_FAILURE);

  return mControls->IndexOfControl(aControl, aIndex);
}

NS_IMETHODIMP
nsHTMLFormElement::SetCurrentRadioButton(const nsAString& aName,
                                         nsIDOMHTMLInputElement* aRadio)
{
  NS_ENSURE_TRUE(mSelectedRadioButtons.Put(aName, aRadio),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetCurrentRadioButton(const nsAString& aName,
                                         nsIDOMHTMLInputElement** aRadio)
{
  mSelectedRadioButtons.Get(aName, aRadio);

  return NS_OK;
}NS_IMETHODIMP
nsHTMLFormElement::GetNextRadioButton(const nsAString& aName,
                                      const PRBool aPrevious,
                                      nsIDOMHTMLInputElement*  aFocusedRadio,
                                      nsIDOMHTMLInputElement** aRadioOut)
{
  // Get Next (aGetAdjacentInDir == 1) or previous (-1) radio button.
  // Return the radio button relative to the focused radio button.
  // If no radio is focused, get the radio relative to the selected one.
  *aRadioOut = nsnull;

  nsCOMPtr<nsIDOMHTMLInputElement> currentRadio;
  if (aFocusedRadio) {
    currentRadio = aFocusedRadio;
  }
  else {
    mSelectedRadioButtons.Get(aName, getter_AddRefs(currentRadio));
  }

  nsCOMPtr<nsISupports> itemWithName;
  ResolveName(aName, getter_AddRefs(itemWithName));
  nsCOMPtr<nsIDOMNodeList> radioNodeList(do_QueryInterface(itemWithName));

  // XXX If ResolveName could return an nsContentList instead then we 
  //     could get an nsIContentList instead of using this hacky upcast

  nsBaseContentList *radioGroup =
    NS_STATIC_CAST(nsBaseContentList *, (nsIDOMNodeList *)radioNodeList);
  if (!radioGroup) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> currentRadioNode(do_QueryInterface(currentRadio));
  NS_ASSERTION(currentRadioNode, "No nsIContent for current radio button");
  PRInt32 index = radioGroup->IndexOf(currentRadioNode, PR_TRUE);
  if (index < 0) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 numRadios;
  radioGroup->GetLength(&numRadios);
  PRBool disabled;
  nsCOMPtr<nsIDOMHTMLInputElement> radio;

  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios -1;
      }
    }
    else if (++index >= numRadios) {
      index = 0;
    }
    nsCOMPtr<nsIDOMNode> radioDOMNode;
    radioGroup->Item(index, getter_AddRefs(radioDOMNode));
    radio = do_QueryInterface(radioDOMNode);
    NS_ASSERTION(radio, "mRadioButtons holding a non-radio button");
    radio->GetDisabled(&disabled);
  } while (disabled && radio != currentRadio);

  NS_IF_ADDREF(*aRadioOut = radio);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::WalkRadioGroup(const nsAString& aName,
                                  nsIRadioVisitor* aVisitor)
{
  nsresult rv = NS_OK;

  PRBool stopIterating = PR_FALSE;

  if (aName.IsEmpty()) {
    //
    // XXX If the name is empty, it's not stored in the control list.  There
    // *must* be a more efficient way to do this.
    //
    nsCOMPtr<nsIFormControl> control;
    PRUint32 len = 0;
    GetElementCount(&len);
    for (PRUint32 i=0; i<len; i++) {
      GetElementAt(i, getter_AddRefs(control));
      if (control->GetType() == NS_FORM_INPUT_RADIO) {
        nsCOMPtr<nsIContent> controlContent(do_QueryInterface(control));
        if (controlContent) {
          //
          // XXX This is a particularly frivolous string copy just to determine
          // if the string is empty or not
          //
          nsAutoString name;
          if (controlContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name,
                                      name) != NS_CONTENT_ATTR_NOT_THERE &&
              name.IsEmpty()) {
            aVisitor->Visit(control, &stopIterating);
            if (stopIterating) {
              break;
            }
          }
        }
      }
    }
  } else {
    //
    // Get the control / list of controls from the form using form["name"]
    //
    nsCOMPtr<nsISupports> item;
    rv = ResolveName(aName, getter_AddRefs(item));

    if (item) {
      //
      // If it's just a lone radio button, then select it.
      //
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(item));
      if (formControl) {
        if (formControl->GetType() == NS_FORM_INPUT_RADIO) {
          aVisitor->Visit(formControl, &stopIterating);
        }
      } else {
        nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(item));
        if (nodeList) {
          PRUint32 length = 0;
          nodeList->GetLength(&length);
          for (PRUint32 i=0; i<length; i++) {
            nsCOMPtr<nsIDOMNode> node;
            nodeList->Item(i, getter_AddRefs(node));
            nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(node));
            if (formControl) {
              if (formControl->GetType() == NS_FORM_INPUT_RADIO) {
                aVisitor->Visit(formControl, &stopIterating);
                if (stopIterating) {
                  break;
                }
              }
            }
          }
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLFormElement::AddToRadioGroup(const nsAString& aName,
                                   nsIFormControl* aRadio)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::RemoveFromRadioGroup(const nsAString& aName,
                                        nsIFormControl* aRadio)
{
  return NS_OK;
}


//----------------------------------------------------------------------
// nsFormControlList implementation, this could go away if there were
// a lightweight collection implementation somewhere

nsFormControlList::nsFormControlList(nsIDOMHTMLFormElement* aForm) :
  mForm(aForm)
{
}

nsFormControlList::~nsFormControlList()
{
  mForm = nsnull;
  Clear();
}

nsresult nsFormControlList::Init()
{
  NS_ENSURE_TRUE(
    mNameLookupTable.Init(NS_FORM_CONTROL_LIST_HASHTABLE_SIZE),
    NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

void
nsFormControlList::SetForm(nsIDOMHTMLFormElement* aForm)
{
  mForm = aForm; // WEAK - the form owns me
}

void
nsFormControlList::Clear()
{
  // Null out childrens' pointer to me.  No refcounting here
  PRInt32 i;
  for (i = mElements.Count()-1; i >= 0; i--) {
    nsIFormControl* f = NS_STATIC_CAST(nsIFormControl *,
                                       mElements.ElementAt(i));
    if (f) {
      f->SetForm(nsnull, PR_FALSE); 
    }
  }
  mElements.Clear();

  for (i = mNotInElements.Count()-1; i >= 0; i--) {
    nsIFormControl* f = NS_STATIC_CAST(nsIFormControl*,
                                       mNotInElements.ElementAt(i));
    if (f) {
      f->SetForm(nsnull, PR_FALSE);
    }
  }
  mNotInElements.Clear();

  mNameLookupTable.Clear();
}


// XPConnect interface list for nsFormControlList
NS_INTERFACE_MAP_BEGIN(nsFormControlList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFormControlList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFormControlCollection)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsFormControlList)
NS_IMPL_RELEASE(nsFormControlList)


// nsIDOMHTMLCollection interface

NS_IMETHODIMP    
nsFormControlList::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsIFormControl *control = NS_STATIC_CAST(nsIFormControl *,
                                           mElements.SafeElementAt(aIndex));
  if (control) {
    return CallQueryInterface(control, aReturn);
  }

  *aReturn = nsnull;

  return NS_OK;
}

nsresult
nsFormControlList::GetNamedObject(const nsAString& aName,
                                  nsISupports** aResult)
{
  *aResult = nsnull;

  if (!mForm) {
    // No form, no named objects
    return NS_OK;
  }
  
  // Get the hash entry
  mNameLookupTable.Get(aName, aResult);

  return NS_OK;
}

NS_IMETHODIMP 
nsFormControlList::NamedItem(const nsAString& aName,
                             nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsresult rv = NS_OK;

  nsCOMPtr<nsISupports> supports;
  
  if (!mNameLookupTable.Get(aName, getter_AddRefs(supports))) // key not found
     return rv;

  if (supports) {
    // We found something, check if it's a node
    CallQueryInterface(supports, aReturn);

    if (!*aReturn) {
      // If not, we check if it's a node list.
      nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(supports));
      NS_WARN_IF_FALSE(nodeList, "Huh, what's going one here?");

      if (nodeList) {
        // And since we're only asking for one node here, we return the first
        // one from the list.
        rv = nodeList->Item(0, aReturn);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsFormControlList::NamedItem(const nsAString& aName,
                             nsISupports** aReturn)
{
  mNameLookupTable.Get(aName, aReturn);

  return NS_OK;
}

nsresult
nsFormControlList::AddElementToTable(nsIFormControl* aChild,
                                     const nsAString& aName)
{
  if (!ShouldBeInElements(aChild)) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> supports;
  mNameLookupTable.Get(aName, getter_AddRefs(supports));

  if (!supports) {
    // No entry found, add the form control
    nsCOMPtr<nsISupports> child(do_QueryInterface(aChild));

    NS_ENSURE_TRUE( mNameLookupTable.Put(aName, child), NS_ERROR_FAILURE );
  } else {
    // Found something in the hash, check its type
    nsCOMPtr<nsIContent> content(do_QueryInterface(supports));
    nsCOMPtr<nsIContent> newChild(do_QueryInterface(aChild));

    if (content) {
      // Check if the new content is the same as the one we found in the
      // hash, if it is then we leave it in the hash as it is, this will
      // happen if a form control has both a name and an id with the same
      // value
      if (content == newChild) {
        return NS_OK;
      }

      // Found an element, create a list, add the element to the list and put
      // the list in the hash
      nsBaseContentList *list = new nsBaseContentList();
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

      list->AppendElement(content);

      // Add the new child too
      list->AppendElement(newChild);

      nsCOMPtr<nsISupports> listSupports = do_QueryInterface(list);

      // Replace the element with the list.
      NS_ENSURE_TRUE(mNameLookupTable.Put(aName, listSupports),
                     NS_ERROR_FAILURE);
    } else {
      // There's already a list in the hash, add the child to the list
      nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(supports));
      NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

      // Upcast, uggly, but it works!
      nsBaseContentList *list = NS_STATIC_CAST(nsBaseContentList *,
                                               (nsIDOMNodeList *)nodeList.get());

      PRInt32 oldIndex = list->IndexOf(newChild, PR_FALSE);
      
      // Add the new child only if it's not in our list already
      if (oldIndex < 0) {
        list->AppendElement(newChild);
      }
    }
  }

  return NS_OK;
}

nsresult
nsFormControlList::IndexOfControl(nsIFormControl* aControl,
                                  PRInt32* aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  *aIndex = mElements.IndexOf(aControl);

  return NS_OK;
}

nsresult
nsFormControlList::RemoveElementFromTable(nsIFormControl* aChild,
                                          const nsAString& aName)
{
  if (!ShouldBeInElements(aChild)) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aChild);  
  if (!content) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> supports;

  if (!mNameLookupTable.Get(aName, getter_AddRefs(supports)))
    return NS_OK;

  nsCOMPtr<nsIFormControl> fctrl(do_QueryInterface(supports));

  if (fctrl) {
    // Single element in the hash, just remove it if it's the one
    // we're trying to remove...
    if (fctrl == aChild) {
      mNameLookupTable.Remove(aName);
    }

    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> nodeList(do_QueryInterface(supports));
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

  // Upcast, uggly, but it works!
  nsBaseContentList *list = NS_STATIC_CAST(nsBaseContentList *,
                                           (nsIDOMNodeList *)nodeList.get());

  list->RemoveElement(content);

  PRUint32 length = 0;
  list->GetLength(&length);

  if (!length) {
    // If the list is empty we remove if from our hash, this shouldn't
    // happen tho
    mNameLookupTable.Remove(aName);
  } else if (length == 1) {
    // Only one element left, replace the list in the hash with the
    // single element.
    nsCOMPtr<nsIDOMNode> node;
    list->Item(0, getter_AddRefs(node));

    if (node) {
      nsCOMPtr<nsISupports> tmp(do_QueryInterface(node));
      NS_ENSURE_TRUE(mNameLookupTable.Put(aName, tmp),NS_ERROR_FAILURE);
    }
  }

  return NS_OK;
}

// nsFormControlEnumerator
NS_IMPL_ISUPPORTS1(nsFormControlEnumerator, nsISimpleEnumerator)

nsFormControlEnumerator::nsFormControlEnumerator(nsHTMLFormElement* aForm)
  : mForm(aForm), mElementsIndex(0), mNotInElementsIndex(0)
{

  // Create the sorted mNotInElementsSorted array
  PRInt32 len = aForm->mControls->mNotInElements.Count();
  for (PRInt32 indexToAdd=0; indexToAdd < len; indexToAdd++) {
    // Ref doesn't need to be strong, don't bother making it so
    nsIFormControl* controlToAdd = NS_STATIC_CAST(nsIFormControl*,
        aForm->mControls->mNotInElements.ElementAt(indexToAdd));

    // Go through the array and insert the element at the first place where
    // it is less than the element already in the array
    nsCOMPtr<nsIDOMNode> controlToAddNode = do_QueryInterface(controlToAdd);
    nsCOMPtr<nsIDOMNode> existingNode;
    PRBool inserted = PR_FALSE;
    // Loop over all elements backwards (from indexToAdd to 0)
    // indexToAdd is equal to the array length because we've been adding to it
    // constantly
    PRUint32 i = indexToAdd;
    while (i > 0) {
      i--;
      existingNode = do_QueryElementAt(&mNotInElementsSorted, i);
      PRInt32 comparison;
      if (NS_FAILED(nsHTMLFormElement::CompareNodes(controlToAddNode,
                                                    existingNode,
                                                    &comparison))) {
        break;
      }
      if (comparison > 0) {
        if (mNotInElementsSorted.InsertElementAt(controlToAdd, i+1)) {
          inserted = PR_TRUE;
        }
        break;
      }
    }

    // If it wasn't inserted yet, it is greater than everything in the array
    // and must be appended.
    if (!inserted) {
      if (!mNotInElementsSorted.InsertElementAt(controlToAdd,0)) {
        break;
      }
    }
  }
}

NS_IMETHODIMP
nsFormControlEnumerator::HasMoreElements(PRBool* aHasMoreElements)
{
  PRUint32 len;
  mForm->GetElementCount(&len);
  if (mElementsIndex < len) {
    *aHasMoreElements = PR_TRUE;
  } else {
    PRUint32 notInElementsLen;
    mNotInElementsSorted.Count(&notInElementsLen);
    *aHasMoreElements = mNotInElementsIndex < notInElementsLen;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlEnumerator::GetNext(nsISupports** aNext)
{
  // Holds the current form control in form.elements
  nsCOMPtr<nsIFormControl> formControl;
  // First get the form control in form.elements
  PRUint32 len;
  mForm->GetElementCount(&len);
  if (mElementsIndex < len) {
    mForm->GetElementAt(mElementsIndex, getter_AddRefs(formControl));
  }
  // If there are still controls in mNotInElementsSorted, determine whether said
  // control is before the current control in the array, and if so, choose it
  // instead
  PRUint32 notInElementsLen;
  mNotInElementsSorted.Count(&notInElementsLen);
  if (mNotInElementsIndex < notInElementsLen) {
    // Get the not-in-elements control - weak ref
    
    nsCOMPtr<nsIFormControl> formControl2 =
        do_QueryElementAt(&mNotInElementsSorted, mNotInElementsIndex);

    if (formControl) {
      // Both form controls are there.  We have to compare them and see which
      // one we need to choose right now.
      nsCOMPtr<nsIDOMNode> dom1 = do_QueryInterface(formControl);
      nsCOMPtr<nsIDOMNode> dom2 = do_QueryInterface(formControl2);
      PRInt32 comparison = 0;
      nsresult rv = nsHTMLFormElement::CompareNodes(dom1, dom2, &comparison);
      NS_ENSURE_SUCCESS(rv, rv);
      if (comparison < 0) {
        *aNext = formControl;
        mElementsIndex++;
      } else {
        *aNext = formControl2;
        mNotInElementsIndex++;
      }
    } else {
      *aNext = formControl2;
      mNotInElementsIndex++;
    }
  } else {
    *aNext = formControl;
    mElementsIndex++;
  }

  NS_IF_ADDREF(*aNext);
  return NS_OK;
}
