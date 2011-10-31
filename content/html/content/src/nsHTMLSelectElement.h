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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Olli Pettay <Olli.Pettay@helsinki.fi>
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
#ifndef nsHTMLSelectElement_h___
#define nsHTMLSelectElement_h___

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLFormElement.h"
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
#include "nsLayoutErrors.h"
#include "nsHTMLOptionElement.h"
#include "nsHTMLFormElement.h"

class nsHTMLSelectElement;

/**
 * The collection of options in the select (what you get back when you do
 * select.options in DOM)
 */
class nsHTMLOptionCollection: public nsIDOMHTMLOptionsCollection,
                              public nsIHTMLCollection,
                              public nsWrapperCache
{
public:
  nsHTMLOptionCollection(nsHTMLSelectElement* aSelect);
  virtual ~nsHTMLOptionCollection();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  virtual JSObject* WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                               bool *triedToWrap);

  // nsIDOMHTMLOptionsCollection interface
  NS_DECL_NSIDOMHTMLOPTIONSCOLLECTION

  // nsIDOMHTMLCollection interface, all its methods are defined in
  // nsIDOMHTMLOptionsCollection

  virtual nsIContent* GetNodeAt(PRUint32 aIndex);
  virtual nsINode* GetParentObject();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsHTMLOptionCollection,
                                                         nsIHTMLCollection)

  // Helpers for nsHTMLSelectElement
  /**
   * Insert an option
   * @param aOption the option to insert
   * @param aIndex the index to insert at
   */
  bool InsertOptionAt(nsHTMLOptionElement* aOption, PRUint32 aIndex)
  {
    return !!mElements.InsertElementAt(aIndex, aOption);
  }

  /**
   * Remove an option
   * @param aIndex the index of the option to remove
   */
  void RemoveOptionAt(PRUint32 aIndex)
  {
    mElements.RemoveElementAt(aIndex);
  }

  /**
   * Get the option at the index
   * @param aIndex the index
   * @param aReturn the option returned [OUT]
   */
  nsHTMLOptionElement *ItemAsOption(PRUint32 aIndex)
  {
    return mElements.SafeElementAt(aIndex, nsnull);
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
  bool AppendOption(nsHTMLOptionElement* aOption)
  {
    return !!mElements.AppendElement(aOption);
  }

  /**
   * Drop the reference to the select.  Called during select destruction.
   */
  void DropReference();

  /**
   * Finds the index of a given option element
   *
   * @param aOption the option to get the index of
   * @param aStartIndex the index to start looking at
   * @param aForward TRUE to look forward, FALSE to look backward
   * @return the option index
   */
  nsresult GetOptionIndex(mozilla::dom::Element* aOption,
                          PRInt32 aStartIndex, bool aForward,
                          PRInt32* aIndex);

private:
  /** The list of options (holds strong references) */
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

  void PutOption(PRInt32 aIndex, const nsAString& aValue)
  {
    // If the option is empty, store the index.  If not, store the value.
    if (aValue.IsEmpty()) {
      mIndices.Put(aIndex);
    } else {
      mValues.Put(aValue);
    }
  }

  bool ContainsOption(PRInt32 aIndex, const nsAString& aValue)
  {
    return mValues.Contains(aValue) || mIndices.Contains(aIndex);
  }

private:
  nsCheapStringSet mValues;
  nsCheapInt32Set mIndices;
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
                           nsIContent* aKid, PRUint32 aIndex, bool aNotify);
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

  /** Typesafe, non-refcounting cast from nsIContent.  Cheaper than QI. **/
  static nsHTMLSelectElement* FromContent(nsIContent* aContent)
  {
    if (aContent && aContent->IsHTML(nsGkAtoms::select))
      return static_cast<nsHTMLSelectElement*>(aContent);
    return nsnull;
  }
 
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLFormElement::)
  NS_SCRIPTABLE NS_IMETHOD Click() {
    return nsGenericHTMLFormElement::Click();
  }
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD Focus() {
    return nsGenericHTMLFormElement::Focus();
  }
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLFormElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::GetInnerHTML(aInnerHTML);
  }
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::SetInnerHTML(aInnerHTML);
  }

  // nsIDOMHTMLSelectElement
  NS_DECL_NSIDOMHTMLSELECTELEMENT

  // nsIContent
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 bool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, bool aNotify);

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_SELECT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  virtual bool RestoreState(nsPresState* aState);

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
                            PRInt32 aContentIndex,
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
                               PRInt32 aContentIndex,
                               bool aNotify);

  /**
   * Checks whether an option is disabled (even if it's part of an optgroup)
   *
   * @param aIndex the index of the option to check
   * @return whether the option is disabled
   */
  NS_IMETHOD IsOptionDisabled(PRInt32 aIndex,
                              bool *aIsDisabled NS_OUTPARAM);

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
  NS_IMETHOD SetOptionsSelectedByIndex(PRInt32 aStartIndex,
                                       PRInt32 aEndIndex,
                                       bool aIsSelected,
                                       bool aClearAll,
                                       bool aSetDisabled,
                                       bool aNotify,
                                       bool* aChangedSomething NS_OUTPARAM);

  /**
   * Finds the index of a given option element
   *
   * @param aOption the option to get the index of
   * @param aStartIndex the index to start looking at
   * @param aForward TRUE to look forward, FALSE to look backward
   * @return the option index
   */
  NS_IMETHOD GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                            PRInt32 aStartIndex,
                            bool aForward,
                            PRInt32* aIndex NS_OUTPARAM);

  /** Whether or not there are optgroups in this select */
  NS_IMETHOD GetHasOptGroups(bool* aHasGroups);

  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);
  virtual nsresult BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue, bool aNotify);
  virtual nsresult AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString* aValue, bool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);
  
  virtual nsresult DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren() {
    return mIsDoneAddingChildren;
  }

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLSelectElement,
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

  // nsIConstraintValidation
  nsresult GetValidationMessage(nsAString& aValidationMessage,
                                ValidityStateType aType);

protected:
  friend class nsSafeOptionListMutation;

  // Helper Methods
  /**
   * Check whether the option specified by the index is selected
   * @param aIndex the index
   * @return whether the option at the index is selected
   */
  bool IsOptionSelectedByIndex(PRInt32 aIndex);
  /**
   * Starting with (and including) aStartIndex, find the first selected index
   * and set mSelectedIndex to it.
   * @param aStartIndex the index to start with
   */
  void FindSelectedIndex(PRInt32 aStartIndex, bool aNotify);
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
                        PRInt32 aIndex,
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
                                 PRInt32 aListIndex,
                                 PRInt32 aDepth,
                                 bool aNotify);
  /**
   * Remove option(s) from the options[] array
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start removing options from the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult RemoveOptionsFromList(nsIContent* aOptions,
                                 PRInt32 aListIndex,
                                 PRInt32 aDepth,
                                 bool aNotify);
  /**
   * Insert option(s) into the options[] array (called by InsertOptionsIntoList)
   * @param aOptions the option or optgroup being added
   * @param aInsertIndex the index to start adding options into the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult InsertOptionsIntoListRecurse(nsIContent* aOptions,
                                        PRInt32* aInsertIndex,
                                        PRInt32 aDepth);
  /**
   * Remove option(s) from the options[] array (called by RemoveOptionsFromList)
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start removing options from the list at
   * @param aNumRemoved the number removed so far [OUT]
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult RemoveOptionsFromListRecurse(nsIContent* aOptions,
                                        PRInt32 aRemoveIndex,
                                        PRInt32* aNumRemoved,
                                        PRInt32 aDepth);

  // nsIConstraintValidation
  void UpdateBarredFromConstraintValidation();
  bool IsValueMissing();
  void UpdateValueMissingValidityState();

  /**
   * Find out how deep this content is from the select (1=direct child)
   * @param aContent the content to check
   * @return the depth
   */
  PRInt32 GetContentDepth(nsIContent* aContent);
  /**
   * Get the index of the first option at, under or following the content in
   * the select, or length of options[] if none are found
   * @param aOptions the content
   * @return the index of the first option
   */
  PRInt32 GetOptionIndexAt(nsIContent* aOptions);
  /**
   * Get the next option following the content in question (not at or under)
   * (this could include siblings of the current content or siblings of the
   * parent or children of siblings of the parent).
   * @param aOptions the content
   * @return the index of the next option after the content
   */
  PRInt32 GetOptionIndexAfter(nsIContent* aOptions);
  /**
   * Get the first option index at or under the content in question.
   * @param aOptions the content
   * @return the index of the first option at or under the content
   */
  PRInt32 GetFirstOptionIndex(nsIContent* aOptions);
  /**
   * Get the first option index under the content in question, within the
   * range specified.
   * @param aOptions the content
   * @param aStartIndex the first child to look at
   * @param aEndIndex the child *after* the last child to look at
   * @return the index of the first option at or under the content
   */
  PRInt32 GetFirstChildOptionIndex(nsIContent* aOptions,
                                   PRInt32 aStartIndex,
                                   PRInt32 aEndIndex);

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

    PRInt32 size = 1;
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

  nsresult SetSelectedIndexInternal(PRInt32 aIndex, bool aNotify);

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

  /**
   * Insert aElement before the node given by aBefore
   */
  nsresult Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore = nsnull);

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
  PRUint32  mNonOptionChildren;
  /** The number of optgroups anywhere under the select */
  PRUint32  mOptGroupCount;
  /**
   * The current selected index for selectedIndex (will be the first selected
   * index if multiple are selected)
   */
  PRInt32   mSelectedIndex;
  /**
   * The temporary restore state in case we try to restore before parser is
   * done adding options
   */
  nsCOMPtr<nsSelectState> mRestoreState;
};

#endif
