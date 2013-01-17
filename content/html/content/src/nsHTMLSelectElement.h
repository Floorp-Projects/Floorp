/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLSelectElement_h___
#define nsHTMLSelectElement_h___

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsISelectControlFrame.h"
#include "nsIHTMLCollection.h"
#include "nsIConstraintValidation.h"

// PresState
#include "nsXPCOM.h"
#include "nsPresState.h"
#include "nsIComponentManager.h"
#include "nsCheapSets.h"
#include "nsError.h"
#include "nsHTMLOptGroupElement.h"
#include "nsHTMLOptionElement.h"
#include "nsHTMLFormElement.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/UnionTypes.h"

class nsHTMLSelectElement;

/**
 * The collection of options in the select (what you get back when you do
 * select.options in DOM)
 */
class nsHTMLOptionCollection: public nsIHTMLCollection,
                              public nsIDOMHTMLOptionsCollection,
                              public nsWrapperCache
{
typedef mozilla::dom::HTMLOptionElementOrHTMLOptGroupElement HTMLOptionOrOptGroupElement;
typedef mozilla::dom::HTMLElementOrLong HTMLElementOrLong;
public:
  nsHTMLOptionCollection(nsHTMLSelectElement* aSelect);
  virtual ~nsHTMLOptionCollection();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  // nsIDOMHTMLOptionsCollection interface
  NS_DECL_NSIDOMHTMLOPTIONSCOLLECTION

  // nsIDOMHTMLCollection interface, all its methods are defined in
  // nsIDOMHTMLOptionsCollection

  virtual mozilla::dom::Element* GetElementAt(uint32_t aIndex);
  virtual nsINode* GetParentObject();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsHTMLOptionCollection,
                                                         nsIHTMLCollection)

  // Helpers for nsHTMLSelectElement
  /**
   * Insert an option
   * @param aOption the option to insert
   * @param aIndex the index to insert at
   */
  void InsertOptionAt(nsHTMLOptionElement* aOption, uint32_t aIndex)
  {
    mElements.InsertElementAt(aIndex, aOption);
  }

  /**
   * Remove an option
   * @param aIndex the index of the option to remove
   */
  void RemoveOptionAt(uint32_t aIndex)
  {
    mElements.RemoveElementAt(aIndex);
  }

  /**
   * Get the option at the index
   * @param aIndex the index
   * @param aReturn the option returned [OUT]
   */
  nsHTMLOptionElement *ItemAsOption(uint32_t aIndex)
  {
    return mElements.SafeElementAt(aIndex, nullptr);
  }

  /**
   * Clears out all options
   */
  void Clear()
  {
    mElements.Clear();
  }

  /**
   * Append an option to end of array
   */
  void AppendOption(nsHTMLOptionElement* aOption)
  {
    mElements.AppendElement(aOption);
  }

  /**
   * Drop the reference to the select.  Called during select destruction.
   */
  void DropReference();

  /**
   * Finds the index of a given option element.
   * If the option isn't part of the collection, return NS_ERROR_FAILURE
   * without setting aIndex.
   *
   * @param aOption the option to get the index of
   * @param aStartIndex the index to start looking at
   * @param aForward TRUE to look forward, FALSE to look backward
   * @return the option index
   */
  nsresult GetOptionIndex(mozilla::dom::Element* aOption,
                          int32_t aStartIndex, bool aForward,
                          int32_t* aIndex);

  virtual JSObject* NamedItem(JSContext* aCx, const nsAString& aName,
                              mozilla::ErrorResult& error);

  inline void Add(const HTMLOptionOrOptGroupElement& aElement,
                  const Nullable<HTMLElementOrLong>& aBefore,
                  mozilla::ErrorResult& aError);
  void Remove(int32_t aIndex, mozilla::ErrorResult& aError);
  int32_t GetSelectedIndex(mozilla::ErrorResult& aError);
  void SetSelectedIndex(int32_t aSelectedIndex, mozilla::ErrorResult& aError);
  void IndexedSetter(uint32_t aIndex, nsIDOMHTMLOptionElement *aOption,
                     mozilla::ErrorResult& aError)
  {
    aError = SetOption(aIndex, aOption);
  }
  virtual void GetSupportedNames(nsTArray<nsString>& aNames);

private:
  /** The list of options (holds strong references).  This is infallible, so
   * various members such as InsertOptionAt are also infallible. */
  nsTArray<nsRefPtr<nsHTMLOptionElement> > mElements;
  /** The select element that contains this array */
  nsHTMLSelectElement* mSelect;
};

#define NS_SELECT_STATE_IID                        \
{ /* 4db54c7c-d159-455f-9d8e-f60ee466dbf3 */       \
  0x4db54c7c,                                      \
  0xd159,                                          \
  0x455f,                                          \
  {0x9d, 0x8e, 0xf6, 0x0e, 0xe4, 0x66, 0xdb, 0xf3} \
}

/**
 * The restore state used by select
 */
class nsSelectState : public nsISupports {
public:
  nsSelectState()
  {
  }
  virtual ~nsSelectState()
  {
  }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SELECT_STATE_IID)
  NS_DECL_ISUPPORTS

  void PutOption(int32_t aIndex, const nsAString& aValue)
  {
    // If the option is empty, store the index.  If not, store the value.
    if (aValue.IsEmpty()) {
      mIndices.Put(aIndex);
    } else {
      mValues.Put(aValue);
    }
  }

  bool ContainsOption(int32_t aIndex, const nsAString& aValue)
  {
    return mValues.Contains(aValue) || mIndices.Contains(aIndex);
  }

private:
  nsCheapSet<nsStringHashKey> mValues;
  nsCheapSet<nsUint32HashKey> mIndices;
};

class NS_STACK_CLASS nsSafeOptionListMutation
{
public:
  /**
   * @param aSelect The select element which option list is being mutated.
   *                Can be null.
   * @param aParent The content object which is being mutated.
   * @param aKid    If not null, a new child element is being inserted to
   *                aParent. Otherwise a child element will be removed.
   * @param aIndex  The index of the content object in the parent.
   */
  nsSafeOptionListMutation(nsIContent* aSelect, nsIContent* aParent,
                           nsIContent* aKid, uint32_t aIndex, bool aNotify);
  ~nsSafeOptionListMutation();
  void MutationFailed() { mNeedsRebuild = true; }
private:
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
  static void operator delete(void*, size_t) {}
  /** The select element which option list is being mutated. */
  nsRefPtr<nsHTMLSelectElement> mSelect;
  /** true if the current mutation is the first one in the stack. */
  bool                       mTopLevelMutation;
  /** true if it is known that the option list must be recreated. */
  bool                       mNeedsRebuild;
  /** Option list must be recreated if more than one mutation is detected. */
  nsMutationGuard            mGuard;
};


/**
 * Implementation of &lt;select&gt;
 */
class nsHTMLSelectElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLSelectElement,
                            public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLSelectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);
  virtual ~nsHTMLSelectElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(nsHTMLSelectElement, select)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLSelectElement
  NS_DECL_NSIDOMHTMLSELECTELEMENT

  // nsIContent
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify);
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify);

  // Overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const { return NS_FORM_SELECT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  virtual bool RestoreState(nsPresState* aState);
  virtual bool IsDisabledForEvents(uint32_t aMessage);

  virtual void FieldSetDisabledChanged(bool aNotify);

  nsEventStates IntrinsicState() const;

  /**
   * To be called when stuff is added under a child of the select--but *before*
   * they are actually added.
   *
   * @param aOptions the content that was added (usually just an option, but
   *        could be an optgroup node with many child options)
   * @param aParent the parent the options were added to (could be an optgroup)
   * @param aContentIndex the index where the options are being added within the
   *        parent (if the parent is an optgroup, the index within the optgroup)
   */
  NS_IMETHOD WillAddOptions(nsIContent* aOptions,
                            nsIContent* aParent,
                            int32_t aContentIndex,
                            bool aNotify);

  /**
   * To be called when stuff is removed under a child of the select--but
   * *before* they are actually removed.
   *
   * @param aParent the parent the option(s) are being removed from
   * @param aContentIndex the index of the option(s) within the parent (if the
   *        parent is an optgroup, the index within the optgroup)
   */
  NS_IMETHOD WillRemoveOptions(nsIContent* aParent,
                               int32_t aContentIndex,
                               bool aNotify);

  /**
   * Checks whether an option is disabled (even if it's part of an optgroup)
   *
   * @param aIndex the index of the option to check
   * @return whether the option is disabled
   */
  NS_IMETHOD IsOptionDisabled(int32_t aIndex,
                              bool *aIsDisabled);

  /**
   * Sets multiple options (or just sets startIndex if select is single)
   * and handles notifications and cleanup and everything under the sun.
   * When this method exits, the select will be in a consistent state.  i.e.
   * if you set the last option to false, it will select an option anyway.
   *
   * @param aStartIndex the first index to set
   * @param aEndIndex the last index to set (set same as first index for one
   *        option)
   * @param aIsSelected whether to set the option(s) to true or false
   * @param aClearAll whether to clear all other options (for example, if you
   *        are normal-clicking on the current option)
   * @param aSetDisabled whether it is permissible to set disabled options
   *        (for JavaScript)
   * @param aNotify whether to notify frames and such
   * @return whether any options were actually changed
   */
  NS_IMETHOD SetOptionsSelectedByIndex(int32_t aStartIndex,
                                       int32_t aEndIndex,
                                       bool aIsSelected,
                                       bool aClearAll,
                                       bool aSetDisabled,
                                       bool aNotify,
                                       bool* aChangedSomething);

  /**
   * Finds the index of a given option element
   *
   * @param aOption the option to get the index of
   * @param aStartIndex the index to start looking at
   * @param aForward TRUE to look forward, FALSE to look backward
   * @return the option index
   */
  NS_IMETHOD GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                            int32_t aStartIndex,
                            bool aForward,
                            int32_t* aIndex);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  
  virtual void DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren() {
    return mIsDoneAddingChildren;
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLSelectElement,
                                           nsGenericHTMLFormElement)

  nsHTMLOptionCollection *GetOptions()
  {
    return mOptions;
  }

  static nsHTMLSelectElement *FromSupports(nsISupports *aSupports)
  {
    return static_cast<nsHTMLSelectElement*>(static_cast<nsINode*>(aSupports));
  }

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // nsIConstraintValidation
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);

  /**
   * Insert aElement before the node given by aBefore
   */
  void Add(nsGenericHTMLElement& aElement, nsGenericHTMLElement* aBefore,
           mozilla::ErrorResult& aError);
  void Add(nsGenericHTMLElement& aElement, int32_t aIndex,
           mozilla::ErrorResult& aError)
  {
    // If item index is out of range, insert to last.
    // (since beforeElement becomes null, it is inserted to last)
    nsIContent* beforeContent = mOptions->GetElementAt(aIndex);
    return Add(aElement, nsGenericHTMLElement::FromContentOrNull(beforeContent),
               aError);
  }

protected:
  friend class nsSafeOptionListMutation;

  // Helper Methods
  /**
   * Check whether the option specified by the index is selected
   * @param aIndex the index
   * @return whether the option at the index is selected
   */
  bool IsOptionSelectedByIndex(int32_t aIndex);
  /**
   * Starting with (and including) aStartIndex, find the first selected index
   * and set mSelectedIndex to it.
   * @param aStartIndex the index to start with
   */
  void FindSelectedIndex(int32_t aStartIndex, bool aNotify);
  /**
   * Select some option if possible (generally the first non-disabled option).
   * @return true if something was selected, false otherwise
   */
  bool SelectSomething(bool aNotify);
  /**
   * Call SelectSomething(), but only if nothing is selected
   * @see SelectSomething()
   * @return true if something was selected, false otherwise
   */
  bool CheckSelectSomething(bool aNotify);
  /**
   * Called to trigger notifications of frames and fixing selected index
   *
   * @param aSelectFrame the frame for this content (could be null)
   * @param aIndex the index that was selected or deselected
   * @param aSelected whether the index was selected or deselected
   * @param aChangeOptionState if false, don't do anything to the
   *                           nsHTMLOptionElement at aIndex.  If true, change
   *                           its selected state to aSelected.
   * @param aNotify whether to notify the style system and such
   */
  void OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                        int32_t aIndex,
                        bool aSelected,
                        bool aChangeOptionState,
                        bool aNotify);
  /**
   * Restore state to a particular state string (representing the options)
   * @param aNewSelected the state string to restore to
   */
  void RestoreStateTo(nsSelectState* aNewSelected);

  // Adding options
  /**
   * Insert option(s) into the options[] array and perform notifications
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start adding options into the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult InsertOptionsIntoList(nsIContent* aOptions,
                                 int32_t aListIndex,
                                 int32_t aDepth,
                                 bool aNotify);
  /**
   * Remove option(s) from the options[] array
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start removing options from the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult RemoveOptionsFromList(nsIContent* aOptions,
                                 int32_t aListIndex,
                                 int32_t aDepth,
                                 bool aNotify);
  /**
   * Insert option(s) into the options[] array (called by InsertOptionsIntoList)
   * @param aOptions the option or optgroup being added
   * @param aInsertIndex the index to start adding options into the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult InsertOptionsIntoListRecurse(nsIContent* aOptions,
                                        int32_t* aInsertIndex,
                                        int32_t aDepth);
  /**
   * Remove option(s) from the options[] array (called by RemoveOptionsFromList)
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start removing options from the list at
   * @param aNumRemoved the number removed so far [OUT]
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult RemoveOptionsFromListRecurse(nsIContent* aOptions,
                                        int32_t aRemoveIndex,
                                        int32_t* aNumRemoved,
                                        int32_t aDepth);

  // nsIConstraintValidation
  void UpdateBarredFromConstraintValidation();
  bool IsValueMissing();
  void UpdateValueMissingValidityState();

  /**
   * Find out how deep this content is from the select (1=direct child)
   * @param aContent the content to check
   * @return the depth
   */
  int32_t GetContentDepth(nsIContent* aContent);
  /**
   * Get the index of the first option at, under or following the content in
   * the select, or length of options[] if none are found
   * @param aOptions the content
   * @return the index of the first option
   */
  int32_t GetOptionIndexAt(nsIContent* aOptions);
  /**
   * Get the next option following the content in question (not at or under)
   * (this could include siblings of the current content or siblings of the
   * parent or children of siblings of the parent).
   * @param aOptions the content
   * @return the index of the next option after the content
   */
  int32_t GetOptionIndexAfter(nsIContent* aOptions);
  /**
   * Get the first option index at or under the content in question.
   * @param aOptions the content
   * @return the index of the first option at or under the content
   */
  int32_t GetFirstOptionIndex(nsIContent* aOptions);
  /**
   * Get the first option index under the content in question, within the
   * range specified.
   * @param aOptions the content
   * @param aStartIndex the first child to look at
   * @param aEndIndex the child *after* the last child to look at
   * @return the index of the first option at or under the content
   */
  int32_t GetFirstChildOptionIndex(nsIContent* aOptions,
                                   int32_t aStartIndex,
                                   int32_t aEndIndex);

  /**
   * Get the frame as an nsISelectControlFrame (MAY RETURN NULL)
   * @return the select frame, or null
   */
  nsISelectControlFrame *GetSelectFrame();

  /**
   * Is this a combobox?
   */
  bool IsCombobox() {
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
      return false;
    }

    uint32_t size = 1;
    GetSize(&size);
    return size <= 1;
  }

  /**
   * Helper method for dispatching ContentReset notifications to list
   * and combo box frames.
   */
  void DispatchContentReset();

  /**
   * Rebuilds the options array from scratch as a fallback in error cases.
   */
  void RebuildOptionsArray(bool aNotify);

#ifdef DEBUG
  void VerifyOptionsArray();
#endif

  nsresult SetSelectedIndexInternal(int32_t aIndex, bool aNotify);

  void SetSelectionChanged(bool aValue, bool aNotify);

  /**
   * Return whether an element should have a validity UI.
   * (with :-moz-ui-invalid and :-moz-ui-valid pseudo-classes).
   *
   * @return Whether the element should have a validity UI.
   */
  bool ShouldShowValidityUI() const {
    /**
     * Always show the validity UI if the form has already tried to be submitted
     * but was invalid.
     *
     * Otherwise, show the validity UI if the selection has been changed.
     */
    if (mForm && mForm->HasEverTriedInvalidSubmit()) {
      return true;
    }

    return mSelectionHasChanged;
  }

  /** The options[] array */
  nsRefPtr<nsHTMLOptionCollection> mOptions;
  /** false if the parser is in the middle of adding children. */
  bool            mIsDoneAddingChildren;
  /** true if our disabled state has changed from the default **/
  bool            mDisabledChanged;
  /** true if child nodes are being added or removed.
   *  Used by nsSafeOptionListMutation.
   */
  bool            mMutating;
  /**
   * True if DoneAddingChildren will get called but shouldn't restore state.
   */
  bool            mInhibitStateRestoration;
  /**
   * True if the selection has changed since the element's creation.
   */
  bool            mSelectionHasChanged;
  /**
   * True if the default selected option has been set.
   */
  bool            mDefaultSelectionSet;
  /**
   * True if :-moz-ui-invalid can be shown.
   */
  bool            mCanShowInvalidUI;
  /**
   * True if :-moz-ui-valid can be shown.
   */
  bool            mCanShowValidUI;

  /** The number of non-options as children of the select */
  uint32_t  mNonOptionChildren;
  /** The number of optgroups anywhere under the select */
  uint32_t  mOptGroupCount;
  /**
   * The current selected index for selectedIndex (will be the first selected
   * index if multiple are selected)
   */
  int32_t   mSelectedIndex;
  /**
   * The temporary restore state in case we try to restore before parser is
   * done adding options
   */
  nsCOMPtr<nsSelectState> mRestoreState;
};

void
nsHTMLOptionCollection::Add(const HTMLOptionOrOptGroupElement& aElement,
                            const Nullable<HTMLElementOrLong>& aBefore,
                            mozilla::ErrorResult& aError)
{
  nsGenericHTMLElement& element =
    aElement.IsHTMLOptionElement() ?
    static_cast<nsGenericHTMLElement&>(*aElement.GetAsHTMLOptionElement()) :
    static_cast<nsGenericHTMLElement&>(*aElement.GetAsHTMLOptGroupElement());

  if (aBefore.IsNull()) {
    mSelect->Add(element, (nsGenericHTMLElement*)nullptr, aError);
  } else if (aBefore.Value().IsHTMLElement()) {
    mSelect->Add(element, &aBefore.Value().GetAsHTMLElement(), aError);
  } else {
    mSelect->Add(element, aBefore.Value().GetAsLong(), aError);
  }
}

#endif
