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

#ifndef nsHTMLFormElement_h__
#define nsHTMLFormElement_h__

#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsFormSubmission.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIWebProgressListener.h"
#include "nsIRadioGroupContainer.h"
#include "nsIURI.h"
#include "nsIWeakReferenceUtils.h"
#include "nsPIDOMWindow.h"
#include "nsUnicharUtils.h"
#include "nsThreadUtils.h"
#include "nsInterfaceHashtable.h"
#include "nsDataHashtable.h"

class nsFormControlList;
class nsIMutableArray;

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
  bool KeyEquals(const KeyTypePointer aKey) const
  {
    return mStr.Equals(*aKey, nsCaseInsensitiveStringComparator());
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
      nsAutoString tmKey(*aKey);
      ToLowerCase(tmKey);
      return HashString(tmKey);
  }
  enum { ALLOW_MEMMOVE = true };

private:
  const nsString mStr;
};

class nsHTMLFormElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLFormElement,
                          public nsIWebProgressListener,
                          public nsIForm,
                          public nsIRadioGroupContainer
{
public:
  nsHTMLFormElement(already_AddRefed<nsINodeInfo> aNodeInfo);
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

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIForm
  NS_IMETHOD_(nsIFormControl*) GetElementAt(PRInt32 aIndex) const;
  NS_IMETHOD_(PRUint32) GetElementCount() const;
  NS_IMETHOD_(already_AddRefed<nsISupports>) ResolveName(const nsAString& aName);
  NS_IMETHOD_(PRInt32) IndexOfControl(nsIFormControl* aControl);
  NS_IMETHOD_(nsIFormControl*) GetDefaultSubmitElement() const;

  // nsIRadioGroupContainer
  NS_IMETHOD SetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement* aRadio);
  NS_IMETHOD GetCurrentRadioButton(const nsAString& aName,
                                   nsIDOMHTMLInputElement** aRadio);
  NS_IMETHOD GetPositionInGroup(nsIDOMHTMLInputElement *aRadio,
                                PRInt32 *aPositionIndex,
                                PRInt32 *aItemsInGroup);
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const bool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadioOut);
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            bool aFlushContent);
  NS_IMETHOD AddToRadioGroup(const nsAString& aName,
                             nsIFormControl* aRadio);
  NS_IMETHOD RemoveFromRadioGroup(const nsAString& aName,
                                  nsIFormControl* aRadio);
  virtual PRUint32 GetRequiredRadioCount(const nsAString& aName) const;
  virtual void RadioRequiredChanged(const nsAString& aName,
                                    nsIFormControl* aRadio);
  virtual bool GetValueMissingState(const nsAString& aName) const;
  virtual void SetValueMissingState(const nsAString& aName, bool aValue);

  // nsIContent
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult WillHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString* aValue, bool aNotify);

  /**
   * Forget all information about the current submission (and the fact that we
   * are currently submitting at all).
   */
  void ForgetCurrentSubmission();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLFormElement,
                                                     nsGenericHTMLElement)

  /**
   * Remove an element from this form's list of elements
   *
   * @param aElement the element to remove
   * @param aUpdateValidity If true, updates the form validity.
   * @return NS_OK if the element was successfully removed.
   */
  nsresult RemoveElement(nsGenericHTMLFormElement* aElement,
                         bool aUpdateValidity);

  /**
   * Remove an element from the lookup table maintained by the form.
   * We can't fold this method into RemoveElement() because when
   * RemoveElement() is called it doesn't know if the element is
   * removed because the id attribute has changed, or bacause the
   * name attribute has changed.
   *
   * @param aElement the element to remove
   * @param aName the name or id of the element to remove
   * @return NS_OK if the element was successfully removed.
   */
  nsresult RemoveElementFromTable(nsGenericHTMLFormElement* aElement,
                                  const nsAString& aName);
  /**
   * Add an element to end of this form's list of elements
   *
   * @param aElement the element to add
   * @param aUpdateValidity If true, the form validity will be updated.
   * @param aNotify If true, send nsIDocumentObserver notifications as needed.
   * @return NS_OK if the element was successfully added
   */
  nsresult AddElement(nsGenericHTMLFormElement* aElement, bool aUpdateValidity,
                      bool aNotify);

  /**    
   * Add an element to the lookup table maintained by the form.
   *
   * We can't fold this method into AddElement() because when
   * AddElement() is called, the form control has no
   * attributes.  The name or id attributes of the form control
   * are used as a key into the table.
   */
  nsresult AddElementToTable(nsGenericHTMLFormElement* aChild,
                             const nsAString& aName);
   /**
    * Return whether there is one and only one input text control.
    *
    * @return Whether there is exactly one input text control.
    */
  bool HasSingleTextControl() const;

  /**
   * Check whether a given nsIFormControl is the default submit
   * element.  This is different from just comparing to
   * GetDefaultSubmitElement() in certain situations inside an update
   * when GetDefaultSubmitElement() might not be up to date.  aControl
   * is expected to not be null.
   */
  bool IsDefaultSubmitElement(const nsIFormControl* aControl) const;

  /**
   * Flag the form to know that a button or image triggered scripted form
   * submission. In that case the form will defer the submission until the
   * script handler returns and the return value is known.
   */
  void OnSubmitClickBegin(nsIContent* aOriginatingElement);
  void OnSubmitClickEnd();

  /**
   * This method will update the form validity so the submit controls states
   * will be updated (for -moz-submit-invalid pseudo-class).
   * This method has to be called by form elements whenever their validity state
   * or status regarding constraint validation changes.
   *
   * @note This method isn't used for CheckValidity().
   * @note If an element becomes barred from constraint validation, it has to be
   * considered as valid.
   *
   * @param aElementValidityState the new validity state of the element
   */
  void UpdateValidity(bool aElementValidityState);

  /**
   * Returns the form validity based on the last UpdateValidity() call.
   *
   * @return Whether the form was valid the last time UpdateValidity() was called.
   *
   * @note This method may not return the *current* validity state!
   */
  bool GetValidity() const { return !mInvalidElementsCount; }

  /**
   * This method check the form validity and make invalid form elements send
   * invalid event if needed.
   *
   * @return Whether the form is valid.
   *
   * @note Do not call this method if novalidate/formnovalidate is used.
   * @note This method might disappear with bug 592124, hopefuly.
   */
  bool CheckValidFormSubmission();

  virtual nsXPCClassInfo* GetClassInfo();

  /**
   * Walk over the form elements and call SubmitNamesValues() on them to get
   * their data pumped into the FormSubmitter.
   *
   * @param aFormSubmission the form submission object
   */
  nsresult WalkFormElements(nsFormSubmission* aFormSubmission);

  /**
   * Whether the submission of this form has been ever prevented because of
   * being invalid.
   *
   * @return Whether the submission of this form has been prevented because of
   * being invalid.
   */
  bool HasEverTriedInvalidSubmit() const { return mEverTriedInvalidSubmit; }

protected:
  class RemoveElementRunnable;
  friend class RemoveElementRunnable;

  class RemoveElementRunnable : public nsRunnable {
  public:
    RemoveElementRunnable(nsHTMLFormElement* aForm)
      : mForm(aForm)
    {}

    NS_IMETHOD Run() {
      mForm->HandleDefaultSubmitRemoval();
      return NS_OK;
    }

  private:
    nsRefPtr<nsHTMLFormElement> mForm;
  };

  nsresult DoSubmitOrReset(nsEvent* aEvent,
                           PRInt32 aMessage);
  nsresult DoReset();

  // Async callback to handle removal of our default submit
  void HandleDefaultSubmitRemoval();

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
  nsresult BuildSubmission(nsFormSubmission** aFormSubmission, 
                           nsEvent* aEvent);
  /**
   * Perform the submission (called by DoSubmit and FlushPendingSubmission)
   *
   * @param aFormSubmission the submission object
   */
  nsresult SubmitSubmission(nsFormSubmission* aFormSubmission);

  /**
   * Notify any submit observers of the submit.
   *
   * @param aActionURL the URL being submitted to
   * @param aCancelSubmit out param where submit observers can specify that the
   *        submit should be cancelled.
   */
  nsresult NotifySubmitObservers(nsIURI* aActionURL, bool* aCancelSubmit,
                                 bool aEarlyNotify);

  /**
   * Just like ResolveName(), but takes an arg for whether to flush
   */
  already_AddRefed<nsISupports> DoResolveName(const nsAString& aName, bool aFlushContent);

  /**
   * Get the full URL to submit to.  Do not submit if the returned URL is null.
   *
   * @param aActionURL the full, unadulterated URL you'll be submitting to [OUT]
   * @param aOriginatingElement the originating element of the form submission [IN]
   */
  nsresult GetActionURL(nsIURI** aActionURL, nsIContent* aOriginatingElement);

  /**
   * Check the form validity following this algorithm:
   * http://www.whatwg.org/specs/web-apps/current-work/#statically-validate-the-constraints
   *
   * @param aInvalidElements [out] parameter containing the list of unhandled
   * invalid controls.
   *
   * @return Whether the form is currently valid.
   */
  bool CheckFormValidity(nsIMutableArray* aInvalidElements) const;

public:
  /**
   * Flush a possible pending submission. If there was a scripted submission
   * triggered by a button or image, the submission was defered. This method
   * forces the pending submission to be submitted. (happens when the handler
   * returns false or there is an action/target change in the script)
   */
  void FlushPendingSubmission();
protected:

  //
  // Data members
  //
  /** The list of controls (form.elements as well as stuff not in elements) */
  nsRefPtr<nsFormControlList> mControls;
  /** The currently selected radio button of each group */
  nsInterfaceHashtable<nsStringCaseInsensitiveHashKey,nsIDOMHTMLInputElement> mSelectedRadioButtons;
  /** The number of required radio button of each group */
  nsDataHashtable<nsStringCaseInsensitiveHashKey,PRUint32> mRequiredRadioButtonCounts;
  /** The value missing state of each group */
  nsDataHashtable<nsStringCaseInsensitiveHashKey,bool> mValueMissingRadioGroups;
  /** Whether we are currently processing a submit event or not */
  bool mGeneratingSubmit;
  /** Whether we are currently processing a reset event or not */
  bool mGeneratingReset;
  /** Whether we are submitting currently */
  bool mIsSubmitting;
  /** Whether the submission is to be deferred in case a script triggers it */
  bool mDeferSubmission;
  /** Whether we notified NS_FORMSUBMIT_SUBJECT listeners already */
  bool mNotifiedObservers;
  /** If we notified the listeners early, what was the result? */
  bool mNotifiedObserversResult;
  /** Keep track of what the popup state was when the submit was initiated */
  PopupControlState mSubmitPopupState;
  /** Keep track of whether a submission was user-initiated or not */
  bool mSubmitInitiatedFromUserInput;

  /** The pending submission object */
  nsAutoPtr<nsFormSubmission> mPendingSubmission;
  /** The request currently being submitted */
  nsCOMPtr<nsIRequest> mSubmittingRequest;
  /** The web progress object we are currently listening to */
  nsWeakPtr mWebProgress;

  /** The default submit element -- WEAK */
  nsGenericHTMLFormElement* mDefaultSubmitElement;

  /** The first submit element in mElements -- WEAK */
  nsGenericHTMLFormElement* mFirstSubmitInElements;

  /** The first submit element in mNotInElements -- WEAK */
  nsGenericHTMLFormElement* mFirstSubmitNotInElements;

  /**
   * Number of invalid and candidate for constraint validation elements in the
   * form the last time UpdateValidity has been called.
   * @note Should only be used by UpdateValidity() and GetValidity()!
   */
  PRInt32 mInvalidElementsCount;

  /**
   * Whether the submission of this form has been ever prevented because of
   * being invalid.
   */
  bool mEverTriedInvalidSubmit;

protected:
  /** Detection of first form to notify observers */
  static bool gFirstFormSubmitted;
  /** Detection of first password input to initialize the password manager */
  static bool gPasswordManagerInitialized;
};

#endif // nsHTMLFormElement_h__
