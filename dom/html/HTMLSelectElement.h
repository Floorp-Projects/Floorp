/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLSelectElement_h
#define mozilla_dom_HTMLSelectElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIConstraintValidation.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/HTMLOptionsCollection.h"
#include "mozilla/ErrorResult.h"
#include "nsCheapSets.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "nsContentUtils.h"

class nsContentList;
class nsIDOMHTMLOptionElement;
class nsIHTMLCollection;
class nsISelectControlFrame;
class nsPresState;

namespace mozilla {

class EventChainPostVisitor;
class EventChainPreVisitor;

namespace dom {

class HTMLFormSubmission;
class HTMLSelectElement;

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
class SelectState : public nsISupports
{
public:
  SelectState()
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
  virtual ~SelectState()
  {
  }

  nsCheapSet<nsStringHashKey> mValues;
  nsCheapSet<nsUint32HashKey> mIndices;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SelectState, NS_SELECT_STATE_IID)

class MOZ_STACK_CLASS SafeOptionListMutation
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
  SafeOptionListMutation(nsIContent* aSelect, nsIContent* aParent,
                         nsIContent* aKid, uint32_t aIndex, bool aNotify);
  ~SafeOptionListMutation();
  void MutationFailed() { mNeedsRebuild = true; }
private:
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
  static void operator delete(void*, size_t) {}
  /** The select element which option list is being mutated. */
  RefPtr<HTMLSelectElement> mSelect;
  /** true if the current mutation is the first one in the stack. */
  bool                       mTopLevelMutation;
  /** true if it is known that the option list must be recreated. */
  bool                       mNeedsRebuild;
  /** Whether we should be notifying when we make various method calls on
      mSelect */
  const bool                 mNotify;
  /** The selected index at mutation start. */
  int32_t                    mInitialSelectedIndex;
  /** Option list must be recreated if more than one mutation is detected. */
  nsMutationGuard            mGuard;
};


/**
 * Implementation of &lt;select&gt;
 */
class HTMLSelectElement final : public nsGenericHTMLFormElementWithState,
                                public nsIConstraintValidation
{
public:
  /**
   *  IS_SELECTED   whether to set the option(s) to true or false
   *
   *  CLEAR_ALL     whether to clear all other options (for example, if you
   *                are normal-clicking on the current option)
   *
   *  SET_DISABLED  whether it is permissible to set disabled options
   *                (for JavaScript)
   *
   *  NOTIFY        whether to notify frames and such
   *
   *  NO_RESELECT   no need to select something after an option is deselected
   *                (for reset)
   */
  enum OptionType {
    IS_SELECTED   = 1 << 0,
    CLEAR_ALL     = 1 << 1,
    SET_DISABLED  = 1 << 2,
    NOTIFY        = 1 << 3,
    NO_RESELECT   = 1 << 4
  };

  using nsIConstraintValidation::GetValidationMessage;

  explicit HTMLSelectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                             FromParser aFromParser = NOT_FROM_PARSER);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLSelectElement, select)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t TabIndexDefault() override;

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override
  {
    return true;
  }

  // WebIdl HTMLSelectElement
  bool Autofocus() const
  {
    return GetBoolAttr(nsGkAtoms::autofocus);
  }
  void SetAutofocus(bool aVal, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::autofocus, aVal, aRv);
  }
  void GetAutocomplete(DOMString& aValue);
  void SetAutocomplete(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::autocomplete, aValue, aRv);
  }

  void GetAutocompleteInfo(AutocompleteInfo& aInfo);

  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  void SetDisabled(bool aVal, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aVal, aRv);
  }
  HTMLFormElement* GetForm() const
  {
    return nsGenericHTMLFormElementWithState::GetForm();
  }
  bool Multiple() const
  {
    return GetBoolAttr(nsGkAtoms::multiple);
  }
  void SetMultiple(bool aVal, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::multiple, aVal, aRv);
  }

  void GetName(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }
  bool Required() const
  {
    return State().HasState(NS_EVENT_STATE_REQUIRED);
  }
  void SetRequired(bool aVal, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::required, aVal, aRv);
  }
  uint32_t Size() const
  {
    return GetUnsignedIntAttr(nsGkAtoms::size, 0);
  }
  void SetSize(uint32_t aSize, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::size, aSize, 0, aRv);
  }

  void GetType(nsAString& aValue);

  HTMLOptionsCollection* Options() const
  {
    return mOptions;
  }
  uint32_t Length() const
  {
    return mOptions->Length();
  }
  void SetLength(uint32_t aLength, ErrorResult& aRv);
  Element* IndexedGetter(uint32_t aIdx, bool& aFound) const
  {
    return mOptions->IndexedGetter(aIdx, aFound);
  }
  HTMLOptionElement* Item(uint32_t aIdx) const
  {
    return mOptions->ItemAsOption(aIdx);
  }
  HTMLOptionElement* NamedItem(const nsAString& aName) const
  {
    return mOptions->GetNamedItem(aName);
  }
  void Add(const HTMLOptionElementOrHTMLOptGroupElement& aElement,
           const Nullable<HTMLElementOrLong>& aBefore,
           ErrorResult& aRv);
  void Remove(int32_t aIndex);
  void IndexedSetter(uint32_t aIndex, HTMLOptionElement* aOption,
                     ErrorResult& aRv)
  {
    mOptions->IndexedSetter(aIndex, aOption, aRv);
  }

  static bool MatchSelectedOptions(Element* aElement, int32_t, nsAtom*,
                                   void*);

  nsIHTMLCollection* SelectedOptions();

  int32_t SelectedIndex() const
  {
    return mSelectedIndex;
  }
  void SetSelectedIndex(int32_t aIdx, ErrorResult& aRv)
  {
    aRv = SetSelectedIndexInternal(aIdx, true);
  }
  void GetValue(DOMString& aValue);
  void SetValue(const nsAString& aValue);

  // Override SetCustomValidity so we update our state properly when it's called
  // via bindings.
  void SetCustomValidity(const nsAString& aError);

  using nsINode::Remove;

  // nsINode
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // nsIContent
  virtual nsresult GetEventTargetParent(
                     EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) override;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable, int32_t* aTabIndex) override;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) override;
  virtual void RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify) override;

  // Overriden nsIFormControl methods
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(HTMLFormSubmission* aFormSubmission) override;
  NS_IMETHOD SaveState() override;
  virtual bool RestoreState(nsPresState* aState) override;
  virtual bool IsDisabledForEvents(EventMessage aMessage) override;

  virtual void FieldSetDisabledChanged(bool aNotify) override;

  EventStates IntrinsicState() const override;

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
                              bool* aIsDisabled);
  bool IsOptionDisabled(HTMLOptionElement* aOption) const;

  /**
   * Sets multiple options (or just sets startIndex if select is single)
   * and handles notifications and cleanup and everything under the sun.
   * When this method exits, the select will be in a consistent state.  i.e.
   * if you set the last option to false, it will select an option anyway.
   *
   * @param aStartIndex the first index to set
   * @param aEndIndex the last index to set (set same as first index for one
   *        option)
   * @param aOptionsMask determines whether to set, clear all or disable
   *        options and whether frames are to be notified of such.
   * @return whether any options were actually changed
   */
  bool SetOptionsSelectedByIndex(int32_t aStartIndex,
                                 int32_t aEndIndex,
                                 uint32_t aOptionsMask);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  virtual void DoneAddingChildren(bool aHaveNotified) override;
  virtual bool IsDoneAddingChildren() override {
    return mIsDoneAddingChildren;
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  virtual nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                              int32_t aModType) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                         bool aPreallocateChildren) const override;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLSelectElement,
                                           nsGenericHTMLFormElementWithState)

  HTMLOptionsCollection* GetOptions()
  {
    return mOptions;
  }

  // nsIConstraintValidation
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType) override;

  void UpdateValueMissingValidityState();
  /**
   * Insert aElement before the node given by aBefore
   */
  void Add(nsGenericHTMLElement& aElement, nsGenericHTMLElement* aBefore,
           ErrorResult& aError);
  void Add(nsGenericHTMLElement& aElement, int32_t aIndex, ErrorResult& aError)
  {
    // If item index is out of range, insert to last.
    // (since beforeElement becomes null, it is inserted to last)
    nsIContent* beforeContent = mOptions->GetElementAt(aIndex);
    return Add(aElement, nsGenericHTMLElement::FromContentOrNull(beforeContent),
               aError);
  }

  /**
   * Is this a combobox?
   */
  bool IsCombobox() const
  {
    return !Multiple() && Size() <= 1;
  }

  bool OpenInParentProcess();
  void SetOpenInParentProcess(bool aVal);

  void GetPreviewValue(nsAString& aValue)
  {
    aValue = mPreviewValue;
  }
  void SetPreviewValue(const nsAString& aValue);

protected:
  virtual ~HTMLSelectElement();

  friend class SafeOptionListMutation;

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
   *                           HTMLOptionElement at aIndex.  If true, change
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
  void RestoreStateTo(SelectState* aNewSelected);

  // Adding options
  /**
   * Insert option(s) into the options[] array and perform notifications
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start adding options into the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  void InsertOptionsIntoList(nsIContent* aOptions,
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

  // nsIConstraintValidation
  void UpdateBarredFromConstraintValidation();
  bool IsValueMissing() const;

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
   * Get the frame as an nsISelectControlFrame (MAY RETURN nullptr)
   * @return the select frame, or null
   */
  nsISelectControlFrame* GetSelectFrame();

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
   * Marks the selectedOptions list as dirty, so that it'll populate itself
   * again.
   */
  void UpdateSelectedOptions();

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
  RefPtr<HTMLOptionsCollection> mOptions;
  nsContentUtils::AutocompleteAttrState mAutocompleteAttrState;
  nsContentUtils::AutocompleteAttrState mAutocompleteInfoState;
  /** false if the parser is in the middle of adding children. */
  bool            mIsDoneAddingChildren;
  /** true if our disabled state has changed from the default **/
  bool            mDisabledChanged;
  /** true if child nodes are being added or removed.
   *  Used by SafeOptionListMutation.
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
  nsCOMPtr<SelectState> mRestoreState;

  /**
   * The live list of selected options.
  */
  RefPtr<nsContentList> mSelectedOptions;

  /**
   * The current displayed preview text.
  */
  nsString  mPreviewValue;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSelectElement_h
