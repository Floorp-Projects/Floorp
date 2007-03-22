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
#include "nsGenericHTMLElement.h"
#include "nsEventStateManager.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsInterfaceHashtable.h"
#include "nsContentList.h"
#include "nsGUIEvent.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

// form submission
#include "nsIFormSubmitObserver.h"
#include "nsIURI.h"
#include "nsIObserverService.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsISimpleEnumerator.h"
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

#include "nsLayoutUtils.h"

#include "nsUnicharUtils.h"
#include "nsEventDispatcher.h"

static const int NS_FORM_CONTROL_LIST_HASHTABLE_SIZE = 16;

class nsFormControlList;

/**
 * hashkey wrapper using nsAString KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsStringCaseInsensitiveHashKey : public PLDHashEntryHdr
{
public:
  typedef const nsAString& KeyType;
  typedef const nsAString* KeyTypePointer;
  nsStringCaseInsensitiveHashKey(KeyTypePointer aStr) : mStr(*aStr) { } //take it easy just deal HashKey 
  nsStringCaseInsensitiveHashKey(const nsStringCaseInsensitiveHashKey& toCopy) : mStr(toCopy.mStr) { }
  ~nsStringCaseInsensitiveHashKey() { }

  KeyType GetKey() const { return mStr; }
  KeyTypePointer GetKeyPointer() const { return &mStr; }
  PRBool KeyEquals(const KeyTypePointer aKey) const
  {
    return mStr.Equals(*aKey,nsCaseInsensitiveStringComparator());
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
      nsAutoString tmKey(*aKey);
      ToLowerCase(tmKey);
      return HashString(tmKey);
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const nsString mStr;
};


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
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

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
  NS_IMETHOD AddElement(nsIFormControl* aElement,
                        PRBool aNotify);
  NS_IMETHOD AddElementToTable(nsIFormControl* aChild,
                               const nsAString& aName);
  NS_IMETHOD GetElementAt(PRInt32 aIndex, nsIFormControl** aElement) const;
  NS_IMETHOD GetElementCount(PRUint32* aCount) const;
  NS_IMETHOD RemoveElement(nsIFormControl* aElement,
                           PRBool aNotify);
  NS_IMETHOD RemoveElementFromTable(nsIFormControl* aElement,
                                    const nsAString& aName);
  NS_IMETHOD ResolveName(const nsAString& aName,
                         nsISupports** aReturn);
  NS_IMETHOD IndexOfControl(nsIFormControl* aControl, PRInt32* aIndex);
  NS_IMETHOD OnSubmitClickBegin();
  NS_IMETHOD OnSubmitClickEnd();
  NS_IMETHOD FlushPendingSubmission();
  NS_IMETHOD ForgetPendingSubmission();
  NS_IMETHOD GetActionURL(nsIURI** aActionURL);
  NS_IMETHOD GetSortedControls(nsTArray<nsIFormControl*>& aControls) const;
  NS_IMETHOD_(nsIFormControl*) GetDefaultSubmitElement() const;
  NS_IMETHOD_(PRBool) HasSingleTextControl() const;

  // nsIRadioGroupContainer
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio);
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio);
  NS_IMETHOD GetPositionInGroup(nsIDOMHTMLInputElement *aRadio,
                                PRInt32 *aPositionIndex,
                                PRInt32 *aItemsInGroup);
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const PRBool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadioOut);
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            PRBool aFlushContent);
  NS_IMETHOD AddToRadioGroup(const nsAString& aName,
                             nsIFormControl* aRadio);
  NS_IMETHOD RemoveFromRadioGroup(const nsAString& aName,
                                  nsIFormControl* aRadio);

  // nsIContent
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
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

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLFormElement,
                                                     nsGenericHTMLElement)

protected:
  nsresult DoSubmitOrReset(nsEvent* aEvent,
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
  nsresult DoSubmit(nsEvent* aEvent);

  /**
   * Prepare the submission object (called by DoSubmit)
   *
   * @param aFormSubmission the submission object
   * @param aEvent the DOM event that was passed to us for the submit
   */
  nsresult BuildSubmission(nsCOMPtr<nsIFormSubmission>& aFormSubmission, 
                           nsEvent* aEvent);
  /**
   * Perform the submission (called by DoSubmit and FlushPendingSubmission)
   *
   * @param aFormSubmission the submission object
   */
  nsresult SubmitSubmission(nsIFormSubmission* aFormSubmission);
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
   * Notify any submit observers of the submit.
   *
   * @param aActionURL the URL being submitted to
   * @param aCancelSubmit out param where submit observers can specify that the
   *        submit should be cancelled.
   */
  nsresult NotifySubmitObservers(nsIURI* aActionURL, PRBool* aCancelSubmit,
                                 PRBool aEarlyNotify);

  /**
   * Just like ResolveName(), but takes an arg for whether to flush
   */
  nsresult DoResolveName(const nsAString& aName, PRBool aFlushContent,
                         nsISupports** aReturn);

  /**
   * Reset the default submit element after the previous default submit control
   * is removed.
   *
   * Note that this, in the worst case, needs to run through all the form's
   * controls.
   *
   * @param aNotify If true, send nsIDocumentObserver notifications on the
   *                new and previous default elements.
   * @param aPrevDefaultInElements If true, the previous default submit element
   *                               was in the elements list. If false, it was
   *                               in the not-in-elements list.
   * @param aPrevDefaultIndex The index of the previous default submit control
   *                          in the list determined by aPrevDefaultInElements.
   */
  void ResetDefaultSubmitElement(PRBool aNotify,
                                 PRBool aPrevDefaultInElements,
                                 PRUint32 aPrevDefaultIndex);
  
  /**
   * Locates the default submit control. This is defined as the first element
   * in document order which can submit a form. Returns null if there are no
   * submit controls.
   *
   * Note that this, in the worst case, needs to run through all the form's
   * controls.
   *
   * @param aPrevDefaultInElements If true, the previous default submit element
   *                               was in the elements list. If false, it was
   *                               in the not-in-elements list.
   * @param aPrevDefaultIndex The index of the previous default submit control
   *                          in the list determined by aPrevDefaultInElements.
   * @return The default submit control for the form, or null if one does
   *         not exist.
   */
  nsIFormControl* FindDefaultSubmit(PRBool aPrevDefaultInElements,
                                    PRUint32 aPrevDefaultIndex) const;

  //
  // Data members
  //
  /** The list of controls (form.elements as well as stuff not in elements) */
  nsRefPtr<nsFormControlList> mControls;
  /** The currently selected radio button of each group */
  nsInterfaceHashtable<nsStringCaseInsensitiveHashKey,nsIDOMHTMLInputElement> mSelectedRadioButtons;
  /** Whether we are currently processing a submit event or not */
  PRPackedBool mGeneratingSubmit;
  /** Whether we are currently processing a reset event or not */
  PRPackedBool mGeneratingReset;
  /** Whether we are submitting currently */
  PRPackedBool mIsSubmitting;
  /** Whether the submission is to be deferred in case a script triggers it */
  PRPackedBool mDeferSubmission;
  /** Whether we notified NS_FORMSUBMIT_SUBJECT listeners already */
  PRPackedBool mNotifiedObservers;
  /** If we notified the listeners early, what was the result? */
  PRPackedBool mNotifiedObserversResult;
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

  /** The default submit element -- WEAK */
  nsIFormControl* mDefaultSubmitElement;

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
  nsFormControlList(nsHTMLFormElement* aForm);
  virtual ~nsFormControlList();

  nsresult Init();

  void DropFormReference();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMHTMLCollection interface
  NS_DECL_NSIDOMHTMLCOLLECTION

  // nsIDOMNSHTMLFormControlList interface
  NS_DECL_NSIDOMNSHTMLFORMCONTROLLIST

  nsresult AddElementToTable(nsIFormControl* aChild,
                             const nsAString& aName);
  nsresult RemoveElementFromTable(nsIFormControl* aChild,
                                  const nsAString& aName);
  nsresult IndexOfControl(nsIFormControl* aControl,
                          PRInt32* aIndex);

  void NamedItemInternal(const nsAString& aName, PRBool aFlushContent,
                         nsISupports **aResult);
  
  /**
   * Create a sorted list of form control elements. This list is sorted
   * in document order and contains the controls in the mElements and
   * mNotInElements list. This function does not add references to the
   * elements.
   *
   * @param aControls The list of sorted controls[out].
   * @return NS_OK or NS_ERROR_OUT_OF_MEMORY.
   */
  nsresult GetSortedControls(nsTArray<nsIFormControl*>& aControls) const;

  nsHTMLFormElement* mForm;  // WEAK - the form owns me

  nsTArray<nsIFormControl*> mElements;  // Holds WEAK references - bug 36639

  // This array holds on to all form controls that are not contained
  // in mElements (form.elements in JS, see ShouldBeInFormControl()).
  // This is needed to properly clean up the bi-directional references
  // (both weak and strong) between the form and its form controls.

  nsTArray<nsIFormControl*> mNotInElements; // Holds WEAK references

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFormControlList,
                                           nsIDOMHTMLCollection)

protected:
  // Drop all our references to the form elements
  void Clear();

  // Flush out the content model so it's up to date.
  void FlushPendingNotifications();
  
  // A map from an ID or NAME attribute to the form control(s), this
  // hash holds strong references either to the named form control, or
  // to a list of named form controls, in the case where this hash
  // holds on to a list of named form controls the list has weak
  // references to the form control.

  nsInterfaceHashtable<nsStringHashKey,nsISupports> mNameLookupTable;
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
nsGenericHTMLElement*
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
    mNotifiedObservers(PR_FALSE),
    mNotifiedObserversResult(PR_FALSE),
    mSubmitPopupState(openAbused),
    mSubmitInitiatedFromUserInput(PR_FALSE),
    mPendingSubmission(nsnull),
    mSubmittingRequest(nsnull),
    mDefaultSubmitElement(nsnull)
{
}

nsHTMLFormElement::~nsHTMLFormElement()
{
  if (mControls) {
    mControls->DropFormReference();
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
    mControls = nsnull;
    return rv;
  }
  
  NS_ENSURE_TRUE(mSelectedRadioButtons.Init(4),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}


// nsISupports

PR_STATIC_CALLBACK(PLDHashOperator)
ElementTraverser(const nsAString& key, nsIDOMHTMLInputElement* element,
                 void* userArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, userArg);
 
  cb->NoteXPCOMChild(element);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLFormElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLFormElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mControls,
                                                       nsIDOMHTMLCollection)
  tmp->mSelectedRadioButtons.EnumerateRead(ElementTraverser, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLFormElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLFormElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLFormElement
NS_HTML_CONTENT_CC_INTERFACE_MAP_BEGIN(nsHTMLFormElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIForm)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIRadioGroupContainer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFormElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLFormElement

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsHTMLFormElement)

NS_IMETHODIMP
nsHTMLFormElement::GetElements(nsIDOMHTMLCollection** aElements)
{
  *aElements = mControls;
  NS_ADDREF(*aElements);
  return NS_OK;
}

nsresult
nsHTMLFormElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
{
  if ((aName == nsGkAtoms::action || aName == nsGkAtoms::target) &&
      aNameSpaceID == kNameSpaceID_None) {
    if (mPendingSubmission) {
      // aha, there is a pending submission that means we're in
      // the script and we need to flush it. let's tell it
      // that the event was ignored to force the flush.
      // the second argument is not playing a role at all.
      FlushPendingSubmission();
    }
    // Don't forget we've notified the password manager already if the
    // page sets the action/target in the during submit. (bug 343182)
    PRBool notifiedObservers = mNotifiedObservers;
    ForgetCurrentSubmission();
    mNotifiedObservers = notifiedObservers;
  }
  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                       aNotify);
}

NS_IMPL_STRING_ATTR(nsHTMLFormElement, AcceptCharset, acceptcharset)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Enctype, enctype)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Method, method)
NS_IMPL_STRING_ATTR(nsHTMLFormElement, Name, name)

NS_IMETHODIMP
nsHTMLFormElement::GetAction(nsAString& aValue)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::action, aValue);
  if (aValue.IsEmpty()) {
    // Avoid resolving action="" to the base uri, bug 297761.
    return NS_OK;
  }
  return GetURIAttr(nsGkAtoms::action, nsnull, aValue);
}

NS_IMETHODIMP
nsHTMLFormElement::SetAction(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::action, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLFormElement::GetTarget(nsAString& aValue)
{
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue)) {
    GetBaseTarget(aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::SetTarget(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::target, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLFormElement::Submit()
{
  // Send the submit event
  nsresult rv = NS_OK;
  nsCOMPtr<nsPresContext> presContext = GetPresContext();
  if (mPendingSubmission) {
    // aha, we have a pending submission that was not flushed
    // (this happens when form.submit() is called twice)
    // we have to delete it and build a new one since values
    // might have changed inbetween (we emulate IE here, that's all)
    mPendingSubmission = nsnull;
  }

  rv = DoSubmitOrReset(nsnull, NS_FORM_SUBMIT);
  return rv;
}

NS_IMETHODIMP
nsHTMLFormElement::Reset()
{
  nsFormEvent event(PR_TRUE, NS_FORM_RESET);
  nsEventDispatcher::Dispatch(NS_STATIC_CAST(nsIContent*, this), nsnull,
                              &event);
  return NS_OK;
}

static const nsAttrValue::EnumTable kFormMethodTable[] = {
  { "get", NS_FORM_METHOD_GET },
  { "post", NS_FORM_METHOD_POST },
  { 0 }
};

static const nsAttrValue::EnumTable kFormEnctypeTable[] = {
  { "multipart/form-data", NS_FORM_ENCTYPE_MULTIPART },
  { "application/x-www-form-urlencoded", NS_FORM_ENCTYPE_URLENCODED },
  { "text/plain", NS_FORM_ENCTYPE_TEXTPLAIN },
  { 0 }
};

PRBool
nsHTMLFormElement::ParseAttribute(PRInt32 aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::method) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable);
    }
    if (aAttribute == nsGkAtoms::enctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsresult
nsHTMLFormElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(aDocument));
  if (htmlDoc) {
    htmlDoc->AddedForm();
  }

  return rv;
}

void
nsHTMLFormElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsCOMPtr<nsIHTMLDocument> oldDocument = do_QueryInterface(GetCurrentDoc());

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  if (oldDocument) {
    oldDocument->RemovedForm();
  }     
  ForgetCurrentSubmission();
}

nsresult
nsHTMLFormElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->originalTarget == NS_STATIC_CAST(nsIContent*, this)) {
    PRUint32 msg = aVisitor.mEvent->message;
    if (msg == NS_FORM_SUBMIT) {
      if (mGeneratingSubmit) {
        aVisitor.mCanHandle = PR_FALSE;
        return NS_OK;
      }
      mGeneratingSubmit = PR_TRUE;

      // let the form know that it needs to defer the submission,
      // that means that if there are scripted submissions, the
      // latest one will be deferred until after the exit point of the handler.
      mDeferSubmission = PR_TRUE;
    }
    else if (msg == NS_FORM_RESET) {
      if (mGeneratingReset) {
        aVisitor.mCanHandle = PR_FALSE;
        return NS_OK;
      }
      mGeneratingReset = PR_TRUE;
    }
  }
  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsresult
nsHTMLFormElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->originalTarget == NS_STATIC_CAST(nsIContent*, this)) {
    PRUint32 msg = aVisitor.mEvent->message;
    if (msg == NS_FORM_SUBMIT) {
      // let the form know not to defer subsequent submissions
      mDeferSubmission = PR_FALSE;
    }

    if (aVisitor.mEventStatus == nsEventStatus_eIgnore) {
      switch (msg) {
        case NS_FORM_RESET:
        case NS_FORM_SUBMIT:
        {
          if (mPendingSubmission && msg == NS_FORM_SUBMIT) {
            // tell the form to forget a possible pending submission.
            // the reason is that the script returned true (the event was
            // ignored) so if there is a stored submission, it will miss
            // the name/value of the submitting element, thus we need
            // to forget it and the form element will build a new one
            ForgetPendingSubmission();
          }
          DoSubmitOrReset(aVisitor.mEvent, msg);
        }
        break;
      }
    } else {
      if (msg == NS_FORM_SUBMIT) {
        // tell the form to flush a possible pending submission.
        // the reason is that the script returned false (the event was
        // not ignored) so if there is a stored submission, it needs to
        // be submitted immediatelly.
        FlushPendingSubmission();
      }
    }

    if (msg == NS_FORM_SUBMIT) {
      mGeneratingSubmit = PR_FALSE;
    }
    else if (msg == NS_FORM_RESET) {
      mGeneratingReset = PR_FALSE;
    }
  }
  return NS_OK;
}

nsresult
nsHTMLFormElement::DoSubmitOrReset(nsEvent* aEvent,
                                   PRInt32 aMessage)
{
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
    // Don't submit if we're not in a document.
    if (doc) {
      rv = DoSubmit(aEvent);
    }
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
nsHTMLFormElement::DoSubmit(nsEvent* aEvent)
{
  NS_ASSERTION(!mIsSubmitting, "Either two people are trying to submit or the "
               "previous submit was not properly cancelled by the DocShell");
  NS_ASSERTION(GetCurrentDoc(), "Should never get here without a current doc");
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
  BuildSubmission(submission, aEvent); 

  // XXXbz if the script global is that for an sXBL/XBL2 doc, it won't
  // be a window...
  nsPIDOMWindow *window = GetOwnerDoc()->GetWindow();

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
  return SubmitSubmission(submission); 
}

nsresult
nsHTMLFormElement::BuildSubmission(nsCOMPtr<nsIFormSubmission>& aFormSubmission, 
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
  rv = GetSubmissionFromForm(this, getter_AddRefs(aFormSubmission));
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  //
  // Dump the data into the submission object
  //
  rv = WalkFormElements(aFormSubmission, originatingElement);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  return NS_OK;
}

nsresult
nsHTMLFormElement::SubmitSubmission(nsIFormSubmission* aFormSubmission)
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

  // If there is no link handler, then we won't actually be able to submit.
  nsIDocument* doc = GetCurrentDoc();
  nsCOMPtr<nsISupports> container = doc ? doc->GetContainer() : nsnull;
  nsCOMPtr<nsILinkHandler> linkHandler(do_QueryInterface(container));
  if (!linkHandler) {
    mIsSubmitting = PR_FALSE;
    return NS_OK;
  }

  // javascript URIs are not really submissions; they just call a function.
  // Also, they may synchronously call submit(), and we want them to be able to
  // do so while still disallowing other double submissions. (Bug 139798)
  // Note that any other URI types that are of equivalent type should also be
  // added here.
  // XXXbz this is a mess.  The real issue here is that nsJSChannel sets the
  // LOAD_BACKGROUND flag, so doesn't notify us, compounded by the fact that
  // the JS executes before we forget the submission in OnStateChange on
  // STATE_STOP.  As a result, we have to make sure that we simply pretend
  // we're not submitting when submitting to a JS URL.  That's kinda bogus, but
  // there we are.
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
  PRBool cancelSubmit = PR_FALSE;
  if (mNotifiedObservers) {
    cancelSubmit = mNotifiedObserversResult;
  } else {
    rv = NotifySubmitObservers(actionURI, &cancelSubmit, PR_TRUE);
    NS_ENSURE_SUBMIT_SUCCESS(rv);
  }

  if (cancelSubmit) {
    mIsSubmitting = PR_FALSE;
    return NS_OK;
  }

  cancelSubmit = PR_FALSE;
  rv = NotifySubmitObservers(actionURI, &cancelSubmit, PR_FALSE);
  NS_ENSURE_SUBMIT_SUCCESS(rv);

  if (cancelSubmit) {
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

    rv = aFormSubmission->SubmitTo(actionURI, target, this, linkHandler,
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
    if (pending && !schemeIsJavaScript) {
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
                                         PRBool* aCancelSubmit,
                                         PRBool  aEarlyNotify)
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
  rv = service->EnumerateObservers(aEarlyNotify ?
                                   NS_EARLYFORMSUBMIT_SUBJECT :
                                   NS_FORMSUBMIT_SUBJECT,
                                   getter_AddRefs(theEnum));
  NS_ENSURE_SUCCESS(rv, rv);

  if (theEnum) {
    nsCOMPtr<nsISupports> inst;
    *aCancelSubmit = PR_FALSE;

    // XXXbz what do the submit observers actually want?  The window
    // of the document this is shown in?  Or something else?
    // sXBL/XBL2 issue
    nsCOMPtr<nsPIDOMWindow> window = GetOwnerDoc()->GetWindow();

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


nsresult
nsHTMLFormElement::WalkFormElements(nsIFormSubmission* aFormSubmission,
                                    nsIContent* aSubmitElement)
{
  nsTArray<nsIFormControl*> sortedControls;
  nsresult rv = mControls->GetSortedControls(sortedControls);
  NS_ENSURE_SUCCESS(rv, rv);

  //
  // Walk the list of nodes and call SubmitNamesValues() on the controls
  //
  PRUint32 len = sortedControls.Length();
  for (PRUint32 i = 0; i < len; ++i) {
    // Tell the control to submit its name/value pairs to the submission
    sortedControls[i]->SubmitNamesValues(aFormSubmission, aSubmitElement);
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
  // Converting to unsigned int will handle negative indices.
  if (PRUint32(aIndex) >= mControls->mElements.Length()) {
    *aFormControl = nsnull;
  } else {
    *aFormControl = mControls->mElements[aIndex];
    NS_ADDREF(*aFormControl);
  }

  return NS_OK;
}

// Compares the position of control1 and control2 in the document
//
// returns < 0 if control1 is before control2,
//         > 0 if control1 is after control2,
//         0 otherwise
static PRInt32 CompareFormControlPosition(nsIFormControl *control1, nsIFormControl *control2)
{
  nsCOMPtr<nsIContent> content1 = do_QueryInterface(control1);
  nsCOMPtr<nsIContent> content2 = do_QueryInterface(control2);

  NS_ASSERTION(content1 && content2, "We should be able to QI to nsIContent here!");

  // We check that the parent of both content nodes is non-null here, because
  // newly created form control elements may not be in the parent form
  // element's DOM tree yet. This prevents an assertion in CompareTreePosition.
  // See Bug 267511 for more information.
  if (content1 && content2 && content1->GetParent() && content2->GetParent())
    return nsLayoutUtils::CompareTreePosition(content1, content2);
  
  return 0;
}

NS_IMETHODIMP
nsHTMLFormElement::AddElement(nsIFormControl* aChild,
                              PRBool aNotify)
{
  // Determine whether to add the new element to the elements or
  // the not-in-elements list.
  PRBool childInElements = ShouldBeInElements(aChild);
  nsTArray<nsIFormControl*>& controlList = childInElements ?
      mControls->mElements : mControls->mNotInElements;
  
  PRUint32 count = controlList.Length();
  nsCOMPtr<nsIFormControl> element;
  
  // Optimize most common case where we insert at the end.
  PRBool lastElement = PR_FALSE;
  PRInt32 position = -1;
  if (count > 0) {
    element = controlList[count - 1];
    position = CompareFormControlPosition(aChild, element);
  }

  // If this item comes after the last element, or the elements array is
  // empty, we append to the end. Otherwise, we do a binary search to
  // determine where the element should go.
  if (position >= 0 || count == 0) {
    // WEAK - don't addref
    controlList.AppendElement(aChild);
    lastElement = PR_TRUE;
  }
  else {
    PRInt32 low = 0, mid, high;
    high = count - 1;
      
    while (low <= high) {
      mid = (low + high) / 2;
        
      element = controlList[mid];
      position = CompareFormControlPosition(aChild, element);
      if (position >= 0)
        low = mid + 1;
      else
        high = mid - 1;
    }
      
    // WEAK - don't addref
    controlList.InsertElementAt(low, aChild);
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
 
  // Default submit element handling
  if (aChild->IsSubmitControl()) {
    // The new child is the default submit if there was not previously
    // a default submit element, or if the new child is before the old
    // default submit element.
    // To speed up parsing, the special case of a form that already
    // has a default submit that's in the same list as the new child
    // is ignored, since the new child then cannot be the default
    // submit element.
    nsIFormControl* oldControl = mDefaultSubmitElement;
    if (!mDefaultSubmitElement ||
        ((!lastElement ||
          ShouldBeInElements(mDefaultSubmitElement) != childInElements) &&
         CompareFormControlPosition(aChild, mDefaultSubmitElement) < 0)) {
      mDefaultSubmitElement = aChild;
    }

    // Notify that the state of the previous default submit element has changed
    // if the element which is the default submit element has changed.  The new
    // default submit element is responsible for its own ContentStatesChanged
    // call.
    if (aNotify && oldControl != mDefaultSubmitElement) {
      nsIDocument* document = GetCurrentDoc();
      if (document) {
        MOZ_AUTO_DOC_UPDATE(document, UPDATE_CONTENT_STATE, PR_TRUE);
        nsCOMPtr<nsIContent> oldElement(do_QueryInterface(oldControl));
        document->ContentStatesChanged(oldElement, nsnull,
                                       NS_EVENT_STATE_DEFAULT);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::AddElementToTable(nsIFormControl* aChild,
                                     const nsAString& aName)
{
  return mControls->AddElementToTable(aChild, aName);  
}


NS_IMETHODIMP 
nsHTMLFormElement::RemoveElement(nsIFormControl* aChild,
                                 PRBool aNotify) 
{
  //
  // Remove it from the radio group if it's a radio button
  //
  nsresult rv = NS_OK;
  if (aChild->GetType() == NS_FORM_INPUT_RADIO) {
    nsCOMPtr<nsIRadioControlElement> radio = do_QueryInterface(aChild);
    rv = radio->WillRemoveFromRadioGroup();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Determine whether to remove the child from the elements list
  // or the not in elements list.
  PRBool childInElements = ShouldBeInElements(aChild);
  nsTArray<nsIFormControl*>& controls = childInElements ?
      mControls->mElements :  mControls->mNotInElements;
  
  // Find the index of the child. This will be used later if necessary
  // to find the default submit.
  PRUint32 index = controls.IndexOf(aChild);
  NS_ASSERTION(index != controls.NoIndex, "Child not in controls");

  controls.RemoveElementAt(index);

  if (aChild == mDefaultSubmitElement) {
    // We are removing the default submit, find the new default
    ResetDefaultSubmitElement(aNotify, childInElements, index );
  }

  return rv;
}

void
nsHTMLFormElement::ResetDefaultSubmitElement(PRBool aNotify,
                                             PRBool aPrevDefaultInElements,
                                             PRUint32 aPrevDefaultIndex)
{
  nsIFormControl* oldDefaultSubmit = mDefaultSubmitElement;
  mDefaultSubmitElement = FindDefaultSubmit(aPrevDefaultInElements,
                                            aPrevDefaultIndex);

  // Inform about change.  Note that we dont' notify on the old default submit
  // (which is being removed) because it's either being removed from the DOM or
  // changing attributes in a way that makes it responsible for sending its own
  // notifications.
  if (aNotify && (oldDefaultSubmit || mDefaultSubmitElement)) {
    NS_ASSERTION(mDefaultSubmitElement != oldDefaultSubmit,
                 "Notifying but elements haven't changed.");
    nsIDocument* document = GetCurrentDoc();
    if (document) {
      MOZ_AUTO_DOC_UPDATE(document, UPDATE_CONTENT_STATE, PR_TRUE);
      nsCOMPtr<nsIContent> newElement(do_QueryInterface(mDefaultSubmitElement));
      document->ContentStatesChanged(newElement, nsnull,
                                     NS_EVENT_STATE_DEFAULT);
    }
  }
 }

nsIFormControl*
nsHTMLFormElement::FindDefaultSubmit(PRBool aPrevDefaultInElements,
                                     PRUint32 aPrevDefaultIndex) const
{
  nsIFormControl* defaultSubmit = nsnull;

  // Find the first submit control in the elements list. This list is
  // in document order.
  PRUint32 length = mControls->mElements.Length();

  // If the previous default submit element was in the elements
  // list, begin the search of the elements list at that index. Given
  // that the list is sorted, the new default submit cannot be before
  // the index of the old child as it already would've been the default submit.
  PRUint32 i = aPrevDefaultInElements ? aPrevDefaultIndex : 0;
  for (; i < length; ++i) {
    nsIFormControl* currentControl = mControls->mElements[i];

    if (currentControl->IsSubmitControl()) {
      defaultSubmit = currentControl;
      break;
    }
  }

  // <input type=image> elements are submit controls but are not in the
  // elements list. Search the not-in-elements list to find any submit control
  // that is ordered in the document before any submit element found already.
  // See the comment above for when not to start the search at the first
  // element.
  length = mControls->mNotInElements.Length();
  i = !aPrevDefaultInElements ? aPrevDefaultIndex : 0;
  for (; i < length; ++i) {
    nsIFormControl* currControl = mControls->mNotInElements[i];

    if (currControl->IsSubmitControl()) {
      if (!defaultSubmit ||
          CompareFormControlPosition(currControl, defaultSubmit) < 0) {
        defaultSubmit = currControl;
      }
      break;
    }
  }
  return defaultSubmit;
}

NS_IMETHODIMP
nsHTMLFormElement::RemoveElementFromTable(nsIFormControl* aElement,
                                          const nsAString& aName)
{
  return mControls->RemoveElementFromTable(aElement, aName);
}

NS_IMETHODIMP
nsHTMLFormElement::ResolveName(const nsAString& aName,
                               nsISupports **aResult)
{
  return DoResolveName(aName, PR_TRUE, aResult);
}

nsresult
nsHTMLFormElement::DoResolveName(const nsAString& aName,
                                 PRBool aFlushContent,
                                 nsISupports **aResult)
{
  mControls->NamedItemInternal(aName, aFlushContent, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::OnSubmitClickBegin()
{
  mDeferSubmission = PR_TRUE;

  // Prepare to run NotifySubmitObservers early before the
  // scripts on the page get to modify the form data, possibly
  // throwing off any password manager. (bug 257781)
  nsCOMPtr<nsIURI> actionURI;
  nsresult rv;

  rv = GetActionURL(getter_AddRefs(actionURI));
  if (NS_FAILED(rv) || !actionURI)
    return NS_OK;

  //
  // Notify observers of submit
  //
  PRBool cancelSubmit = PR_FALSE;
  rv = NotifySubmitObservers(actionURI, &cancelSubmit, PR_TRUE);
  if (NS_SUCCEEDED(rv)) {
    mNotifiedObservers = PR_TRUE;
    mNotifiedObserversResult = cancelSubmit;
  }

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
  SubmitSubmission(mPendingSubmission);

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
    CheckLoadURIWithPrincipal(NodePrincipal(), actionURL,
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
nsHTMLFormElement::GetSortedControls(nsTArray<nsIFormControl*>& aControls) const
{
  return mControls->GetSortedControls(aControls);
}

NS_IMETHODIMP_(nsIFormControl*)
nsHTMLFormElement::GetDefaultSubmitElement() const
{
  return mDefaultSubmitElement;
}

NS_IMETHODIMP_(PRBool)
nsHTMLFormElement::HasSingleTextControl() const
{
  // Input text controls are always in the elements list.
  PRUint32 numTextControlsFound = 0;
  PRUint32 length = mControls->mElements.Length();
  for (PRUint32 i = 0; i < length && numTextControlsFound < 2; ++i) {
    PRInt32 type = mControls->mElements[i]->GetType();
    if (type == NS_FORM_INPUT_TEXT || type == NS_FORM_INPUT_PASSWORD) {
        numTextControlsFound++;
    }
  }
  return numTextControlsFound == 1;
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
  PRUint32 length;
  nsresult rv = mControls->GetLength(&length);
  *aLength = length;
  return rv;
}

void
nsHTMLFormElement::ForgetCurrentSubmission()
{
  mNotifiedObservers = PR_FALSE;
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
nsHTMLFormElement::IndexOfControl(nsIFormControl* aControl, PRInt32* aIndex)
{
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
}

NS_IMETHODIMP
nsHTMLFormElement::GetPositionInGroup(nsIDOMHTMLInputElement *aRadio,
                                      PRInt32 *aPositionIndex,
                                      PRInt32 *aItemsInGroup)
{
  *aPositionIndex = 0;
  *aItemsInGroup = 1;

  nsAutoString name;
  aRadio->GetName(name);
  if (name.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> itemWithName;
  nsresult rv = ResolveName(name, getter_AddRefs(itemWithName));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMNodeList> radioNodeList(do_QueryInterface(itemWithName));

  // XXX If ResolveName could return an nsContentList instead then we 
  //     could get an nsContentList instead of using this hacky upcast
  nsBaseContentList *radioGroup =
    NS_STATIC_CAST(nsBaseContentList *, (nsIDOMNodeList *)radioNodeList);
  NS_ASSERTION(radioGroup, "No such radio group in this container");
  if (!radioGroup) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> currentRadioNode(do_QueryInterface(aRadio));
  NS_ASSERTION(currentRadioNode, "No nsIContent for current radio button");
  *aPositionIndex = radioGroup->IndexOf(currentRadioNode, PR_TRUE);
  NS_ASSERTION(*aPositionIndex >= 0, "Radio button not found in its own group");
  PRUint32 itemsInGroup;
  radioGroup->GetLength(&itemsInGroup);
  *aItemsInGroup = itemsInGroup;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::GetNextRadioButton(const nsAString& aName,
                                      const PRBool aPrevious,
                                      nsIDOMHTMLInputElement*  aFocusedRadio,
                                      nsIDOMHTMLInputElement** aRadioOut)
{
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
  //     could get an nsContentList instead of using this hacky upcast

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
  PRBool disabled = PR_TRUE;
  nsCOMPtr<nsIDOMHTMLInputElement> radio;
  nsCOMPtr<nsIDOMNode> radioDOMNode;
  nsCOMPtr<nsIFormControl> formControl;

  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios -1;
      }
    }
    else if (++index >= (PRInt32)numRadios) {
      index = 0;
    }
    radioGroup->Item(index, getter_AddRefs(radioDOMNode));
    radio = do_QueryInterface(radioDOMNode);
    if (!radio)
      continue;

    formControl = do_QueryInterface(radio);
    if (!formControl || formControl->GetType() != NS_FORM_INPUT_RADIO)
      continue;

    radio->GetDisabled(&disabled);
  } while (disabled && radio != currentRadio);

  NS_IF_ADDREF(*aRadioOut = radio);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFormElement::WalkRadioGroup(const nsAString& aName,
                                  nsIRadioVisitor* aVisitor,
                                  PRBool aFlushContent)
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
          if (controlContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                          EmptyString(), eCaseMatters)) {
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
    rv = DoResolveName(aName, aFlushContent, getter_AddRefs(item));

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

nsFormControlList::nsFormControlList(nsHTMLFormElement* aForm) :
  mForm(aForm),
  // Initialize the elements list to have an initial capacity
  // of 8 to reduce allocations on small forms.
  mElements(8)
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
nsFormControlList::DropFormReference()
{
  mForm = nsnull;
  Clear();
}

void
nsFormControlList::Clear()
{
  // Null out childrens' pointer to me.  No refcounting here
  PRInt32 i;
  for (i = mElements.Length()-1; i >= 0; i--) {
    mElements[i]->SetForm(nsnull, PR_FALSE, PR_TRUE);
  }
  mElements.Clear();

  for (i = mNotInElements.Length()-1; i >= 0; i--) {
    mNotInElements[i]->SetForm(nsnull, PR_FALSE, PR_TRUE);
  }
  mNotInElements.Clear();

  mNameLookupTable.Clear();
}

void
nsFormControlList::FlushPendingNotifications()
{
  if (mForm) {
    nsIDocument* doc = mForm->GetCurrentDoc();
    if (doc) {
      doc->FlushPendingNotifications(Flush_Content);
    }
  }
}

PR_STATIC_CALLBACK(PLDHashOperator)
ControlTraverser(const nsAString& key, nsISupports* control, void* userArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, userArg);
 
  cb->NoteXPCOMChild(control);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFormControlList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFormControlList)
  tmp->Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFormControlList)
  tmp->mNameLookupTable.EnumerateRead(ControlTraverser, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// XPConnect interface list for nsFormControlList
NS_INTERFACE_MAP_BEGIN(nsFormControlList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFormControlList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFormControlCollection)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsFormControlList)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsFormControlList,
                                          nsIDOMHTMLCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsFormControlList,
                                           nsIDOMHTMLCollection)


// nsIDOMHTMLCollection interface

NS_IMETHODIMP    
nsFormControlList::GetLength(PRUint32* aLength)
{
  FlushPendingNotifications();
  *aLength = mElements.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  FlushPendingNotifications();

  if (aIndex < mElements.Length()) {
    return CallQueryInterface(mElements[aIndex], aReturn);
  }

  *aReturn = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsFormControlList::NamedItem(const nsAString& aName,
                             nsIDOMNode** aReturn)
{
  FlushPendingNotifications();

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
      NS_ASSERTION(nodeList, "Huh, what's going one here?");

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
  NamedItemInternal(aName, PR_TRUE, aReturn);
  return NS_OK;
}

void
nsFormControlList::NamedItemInternal(const nsAString& aName,
                                     PRBool aFlushContent,
                                     nsISupports** aReturn)
{
  if (aFlushContent) {
    FlushPendingNotifications();
  }

  mNameLookupTable.Get(aName, aReturn);
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
  // Note -- not a DOM method; callers should handle flushing themselves
  
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

nsresult
nsFormControlList::GetSortedControls(nsTArray<nsIFormControl*>& aControls) const
{
  aControls.Clear();

  // Merge the elements list and the not in elements list. Both lists are
  // already sorted.
  PRUint32 elementsLen = mElements.Length();
  PRUint32 notInElementsLen = mNotInElements.Length();
  aControls.SetCapacity(elementsLen + notInElementsLen);

  PRUint32 elementsIdx = 0;
  PRUint32 notInElementsIdx = 0;

  while (elementsIdx < elementsLen || notInElementsIdx < notInElementsLen) {
    // Check whether we're done with mElements
    if (elementsIdx == elementsLen) {
      NS_ASSERTION(notInElementsIdx < notInElementsLen,
                   "Should have remaining not-in-elements");
      // Append the remaining mNotInElements elements
      if (!aControls.AppendElements(mNotInElements.Elements() +
                                      notInElementsIdx,
                                    notInElementsLen -
                                      notInElementsIdx)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      break;
    }
    // Check whether we're done with mNotInElements
    if (notInElementsIdx == notInElementsLen) {
      NS_ASSERTION(elementsIdx < elementsLen,
                   "Should have remaining in-elements");
      // Append the remaining mElements elements
      if (!aControls.AppendElements(mElements.Elements() +
                                      elementsIdx,
                                    elementsLen -
                                      elementsIdx)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      break;
    }
    // Both lists have elements left.
    NS_ASSERTION(mElements[elementsIdx] &&
                 mNotInElements[notInElementsIdx],
                 "Should have remaining elements");
    // Determine which of the two elements should be ordered
    // first and add it to the end of the list.
    nsIFormControl* elementToAdd;
    if (CompareFormControlPosition(mElements[elementsIdx],
                                   mNotInElements[notInElementsIdx]) < 0) {
      elementToAdd = mElements[elementsIdx];
      ++elementsIdx;
    } else {
      elementToAdd = mNotInElements[notInElementsIdx];
      ++notInElementsIdx;
    }
    // Add the first element to the list.
    if (!aControls.AppendElement(elementToAdd)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ASSERTION(aControls.Length() == elementsLen + notInElementsLen,
               "Not all form controls were added to the sorted list");
#ifdef DEBUG
  if (!aControls.IsEmpty()) {
    for (PRUint32 i = 0; i < aControls.Length() - 1; ++i) {
      NS_ASSERTION(CompareFormControlPosition(aControls[i],
                     aControls[i + 1]) < 0,
                   "Form controls sorted incorrectly");
    }
  }
#endif
  return NS_OK;
}
