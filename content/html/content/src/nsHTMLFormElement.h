/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLFormElement_h__
#define nsHTMLFormElement_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsFormSubmission.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIWebProgressListener.h"
#include "nsIRadioGroupContainer.h"
#include "nsIWeakReferenceUtils.h"
#include "nsThreadUtils.h"
#include "nsInterfaceHashtable.h"
#include "nsDataHashtable.h"
#include "nsAsyncDOMEvent.h"

// Including 'windows.h' will #define GetClassInfo to something else.
#ifdef XP_WIN
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

class nsFormControlList;
class nsIMutableArray;
class nsIURI;

namespace mozilla {
namespace dom {
class HTMLImageElement;
}
}

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
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLFormElement
  NS_DECL_NSIDOMHTMLFORMELEMENT

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIForm
  NS_IMETHOD_(nsIFormControl*) GetElementAt(int32_t aIndex) const;
  NS_IMETHOD_(uint32_t) GetElementCount() const MOZ_OVERRIDE;
  NS_IMETHOD_(int32_t) IndexOfControl(nsIFormControl* aControl) MOZ_OVERRIDE;
  NS_IMETHOD_(nsIFormControl*) GetDefaultSubmitElement() const MOZ_OVERRIDE;

  // nsIRadioGroupContainer
  void SetCurrentRadioButton(const nsAString& aName,
                             nsIDOMHTMLInputElement* aRadio) MOZ_OVERRIDE;
  nsIDOMHTMLInputElement* GetCurrentRadioButton(const nsAString& aName) MOZ_OVERRIDE;
  NS_IMETHOD GetNextRadioButton(const nsAString& aName,
                                const bool aPrevious,
                                nsIDOMHTMLInputElement*  aFocusedRadio,
                                nsIDOMHTMLInputElement** aRadioOut) MOZ_OVERRIDE;
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            bool aFlushContent) MOZ_OVERRIDE;
  void AddToRadioGroup(const nsAString& aName, nsIFormControl* aRadio) MOZ_OVERRIDE;
  void RemoveFromRadioGroup(const nsAString& aName, nsIFormControl* aRadio) MOZ_OVERRIDE;
  virtual uint32_t GetRequiredRadioCount(const nsAString& aName) const MOZ_OVERRIDE;
  virtual void RadioRequiredChanged(const nsAString& aName,
                                    nsIFormControl* aRadio) MOZ_OVERRIDE;
  virtual bool GetValueMissingState(const nsAString& aName) const MOZ_OVERRIDE;
  virtual void SetValueMissingState(const nsAString& aName, bool aValue) MOZ_OVERRIDE;

  virtual nsEventStates IntrinsicState() const MOZ_OVERRIDE;

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult WillHandleEvent(nsEventChainPostVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor) MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  /**
   * Forget all information about the current submission (and the fact that we
   * are currently submitting at all).
   */
  void ForgetCurrentSubmission();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLFormElement,
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
   * @param aRemoveReason describe why this element is removed. If the element
   *        is removed because it's removed from the form, it will be removed
   *        from the past names map too, otherwise it will stay in the past
   *        names map.
   * @return NS_OK if the element was successfully removed.
   */
  enum RemoveElementReason {
    AttributeUpdated,
    ElementRemoved
  };
  nsresult RemoveElementFromTable(nsGenericHTMLFormElement* aElement,
                                  const nsAString& aName,
                                  RemoveElementReason aRemoveReason);

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
   * Remove an image element from this form's list of image elements
   *
   * @param aElement the image element to remove
   * @return NS_OK if the element was successfully removed.
   */
  nsresult RemoveImageElement(mozilla::dom::HTMLImageElement* aElement);

  /**
   * Remove an image element from the lookup table maintained by the form.
   * We can't fold this method into RemoveImageElement() because when
   * RemoveImageElement() is called it doesn't know if the element is
   * removed because the id attribute has changed, or because the
   * name attribute has changed.
   *
   * @param aElement the image element to remove
   * @param aName the name or id of the element to remove
   * @return NS_OK if the element was successfully removed.
   */
  nsresult RemoveImageElementFromTable(mozilla::dom::HTMLImageElement* aElement,
                                      const nsAString& aName,
                                      RemoveElementReason aRemoveReason);
  /**
   * Add an image element to the end of this form's list of image elements
   *
   * @param aElement the element to add
   * @return NS_OK if the element was successfully added
   */
  nsresult AddImageElement(mozilla::dom::HTMLImageElement* aElement);

  /**
   * Add an image element to the lookup table maintained by the form.
   *
   * We can't fold this method into AddImageElement() because when
   * AddImageElement() is called, the image attributes can change.
   * The name or id attributes of the image are used as a key into the table.
   */
  nsresult AddImageElementToTable(mozilla::dom::HTMLImageElement* aChild,
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

  virtual nsXPCClassInfo* GetClassInfo() MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

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

  /**
   * Implements form[name]. Returns form controls in this form with the correct
   * value of the name attribute.
   */
  already_AddRefed<nsISupports>
  FindNamedItem(const nsAString& aName, nsWrapperCache** aCache);

protected:
  void PostPasswordEvent();
  void EventHandled() { mFormPasswordEvent = nullptr; }

  class FormPasswordEvent : public nsAsyncDOMEvent
  {
  public:
    FormPasswordEvent(nsHTMLFormElement* aEventNode,
                      const nsAString& aEventType)
      : nsAsyncDOMEvent(aEventNode, aEventType, true, true)
    {}

    NS_IMETHOD Run() MOZ_OVERRIDE
    {
      static_cast<nsHTMLFormElement*>(mEventNode.get())->EventHandled();
      return nsAsyncDOMEvent::Run();
    }
  };

  nsRefPtr<FormPasswordEvent> mFormPasswordEvent;

  class RemoveElementRunnable;
  friend class RemoveElementRunnable;

  class RemoveElementRunnable : public nsRunnable {
  public:
    RemoveElementRunnable(nsHTMLFormElement* aForm)
      : mForm(aForm)
    {}

    NS_IMETHOD Run() MOZ_OVERRIDE {
      mForm->HandleDefaultSubmitRemoval();
      return NS_OK;
    }

  private:
    nsRefPtr<nsHTMLFormElement> mForm;
  };

  nsresult DoSubmitOrReset(nsEvent* aEvent,
                           int32_t aMessage);
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
   * Find form controls in this form with the correct value in the name
   * attribute.
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

  // Clear the mImageNameLookupTable and mImageElements.
  void Clear();

  // Insert a element into the past names map.
  void AddToPastNamesMap(const nsAString& aName, nsISupports* aChild);

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
  nsDataHashtable<nsStringCaseInsensitiveHashKey,uint32_t> mRequiredRadioButtonCounts;
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

  // This array holds on to all HTMLImageElement(s).
  // This is needed to properly clean up the bi-directional references
  // (both weak and strong) between the form and its HTMLImageElements.

  nsTArray<mozilla::dom::HTMLImageElement*> mImageElements;  // Holds WEAK references

  // A map from an ID or NAME attribute to the HTMLImageElement(s), this
  // hash holds strong references either to the named HTMLImageElement, or
  // to a list of named HTMLImageElement(s), in the case where this hash
  // holds on to a list of named HTMLImageElement(s) the list has weak
  // references to the HTMLImageElement.

  nsInterfaceHashtable<nsStringHashKey,nsISupports> mImageNameLookupTable;

  // A map from names to elements that were gotten by those names from this
  // form in that past.  See "past names map" in the HTML5 specification.

  nsInterfaceHashtable<nsStringHashKey,nsISupports> mPastNameLookupTable;

  /**
   * Number of invalid and candidate for constraint validation elements in the
   * form the last time UpdateValidity has been called.
   * @note Should only be used by UpdateValidity() and GetValidity()!
   */
  int32_t mInvalidElementsCount;

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
