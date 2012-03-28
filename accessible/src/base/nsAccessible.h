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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
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

#ifndef _nsAccessible_H_
#define _nsAccessible_H_

#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/States.h"
#include "nsAccessNodeWrap.h"

#include "nsIAccessible.h"
#include "nsIAccessibleHyperLink.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleValue.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleStates.h"

#include "nsARIAMap.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nsRefPtrHashtable.h"

class AccEvent;
class AccGroupInfo;
class EmbeddedObjCollector;
class KeyBinding;
class nsAccessible;
class nsHyperTextAccessible;
class nsHTMLImageAccessible;
class nsHTMLImageMapAccessible;
class nsHTMLLIAccessible;
struct nsRoleMapEntry;
class Relation;
class nsTextAccessible;

struct nsRect;
class nsIContent;
class nsIFrame;
class nsIAtom;
class nsIView;

typedef nsRefPtrHashtable<nsPtrHashKey<const void>, nsAccessible>
  nsAccessibleHashtable;

// see nsAccessible::GetAttrValue
#define NS_OK_NO_ARIA_VALUE \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x21)

// see nsAccessible::GetNameInternal
#define NS_OK_EMPTY_NAME \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x23)

// see nsAccessible::GetNameInternal
#define NS_OK_NAME_FROM_TOOLTIP \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x25)


#define NS_ACCESSIBLE_IMPL_IID                          \
{  /* 133c8bf4-4913-4355-bd50-426bd1d6e1ad */           \
  0x133c8bf4,                                           \
  0x4913,                                               \
  0x4355,                                               \
  { 0xbd, 0x50, 0x42, 0x6b, 0xd1, 0xd6, 0xe1, 0xad }    \
}

class nsAccessible : public nsAccessNodeWrap, 
                     public nsIAccessible, 
                     public nsIAccessibleHyperLink,
                     public nsIAccessibleSelectable,
                     public nsIAccessibleValue
{
public:
  nsAccessible(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsAccessible();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsAccessible, nsAccessNode)

  NS_DECL_NSIACCESSIBLE
  NS_DECL_NSIACCESSIBLEHYPERLINK
  NS_DECL_NSIACCESSIBLESELECTABLE
  NS_DECL_NSIACCESSIBLEVALUE
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCESSIBLE_IMPL_IID)

  //////////////////////////////////////////////////////////////////////////////
  // nsAccessNode

  virtual void Shutdown();

  //////////////////////////////////////////////////////////////////////////////
  // Public methods

  /**
   * get the description of this accessible
   */
  virtual void Description(nsString& aDescription);

  /**
   * Return DOM node associated with this accessible.
   */
  inline already_AddRefed<nsIDOMNode> DOMNode() const
  {
    nsIDOMNode *DOMNode = nsnull;
    if (GetNode())
      CallQueryInterface(GetNode(), &DOMNode);
    return DOMNode;
  }

  /**
   * Returns the accessible name specified by ARIA.
   */
  nsresult GetARIAName(nsAString& aName);

  /**
   * Maps ARIA state attributes to state of accessible. Note the given state
   * argument should hold states for accessible before you pass it into this
   * method.
   *
   * @param  [in/out] where to fill the states into.
   */
  virtual void ApplyARIAState(PRUint64* aState);

  /**
   * Returns the accessible name provided by native markup. It doesn't take
   * into account ARIA markup used to specify the name.
   *
   * @param  aName             [out] the accessible name
   *
   * @return NS_OK_EMPTY_NAME  points empty name was specified by native markup
   *                           explicitly (see nsIAccessible::name attribute for
   *                           details)
   */
  virtual nsresult GetNameInternal(nsAString& aName);

  /**
   * Return enumerated accessible role (see constants in Role.h).
   */
  inline mozilla::a11y::role Role()
  {
    if (!mRoleMapEntry || mRoleMapEntry->roleRule != kUseMapRole)
      return NativeRole();

    return ARIARoleInternal();
  }

  /**
   * Return true if ARIA role is specified on the element.
   */
  inline bool HasARIARole() const
  {
    return mRoleMapEntry;
  }

  /**
   * Return accessible role specified by ARIA (see constants in
   * roles).
   */
  inline mozilla::a11y::role ARIARole()
  {
    if (!mRoleMapEntry || mRoleMapEntry->roleRule != kUseMapRole)
      return mozilla::a11y::roles::NOTHING;

    return ARIARoleInternal();
  }

  /**
   * Returns enumerated accessible role from native markup (see constants in
   * Role.h). Doesn't take into account ARIA roles.
   */
  virtual mozilla::a11y::role NativeRole();

  /**
   * Return all states of accessible (including ARIA states).
   */
  virtual PRUint64 State();

  /**
   * Return the states of accessible, not taking into account ARIA states.
   * Use State() to get complete set of states.
   */
  virtual PRUint64 NativeState();

  /**
   * Return bit set of invisible and offscreen states.
   */
  PRUint64 VisibilityState();

  /**
   * Returns attributes for accessible without explicitly setted ARIA
   * attributes.
   */
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

  /**
   * Used by ChildAtPoint() method to get direct or deepest child at point.
   */
  enum EWhichChildAtPoint {
    eDirectChild,
    eDeepestChild
  };

  /**
   * Return direct or deepest child at the given point.
   *
   * @param  aX           [in] x coordinate relative screen
   * @param  aY           [in] y coordinate relative screen
   * @param  aWhichChild  [in] flag points if deepest or direct child
   *                        should be returned
   */
  virtual nsAccessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                     EWhichChildAtPoint aWhichChild);

  /**
   * Return the focused child if any.
   */
  virtual nsAccessible* FocusedChild();

  /**
   * Return calculated group level based on accessible hierarchy.
   */
  virtual PRInt32 GetLevelInternal();

  /**
   * Calculate position in group and group size ('posinset' and 'setsize') based
   * on accessible hierarchy.
   *
   * @param  aPosInSet  [out] accessible position in the group
   * @param  aSetSize   [out] the group size
   */
  virtual void GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                          PRInt32 *aSetSize);

  /**
   * Get the relation of the given type.
   */
  virtual Relation RelationByType(PRUint32 aType);

  //////////////////////////////////////////////////////////////////////////////
  // Initializing methods

  /**
   * Set the ARIA role map entry for a new accessible.
   * For a newly created accessible, specify which role map entry should be used.
   *
   * @param aRoleMapEntry The ARIA nsRoleMapEntry* for the accessible, or 
   *                      nsnull if none.
   */
  virtual void SetRoleMapEntry(nsRoleMapEntry *aRoleMapEntry);

  /**
   * Update the children cache.
   */
  inline bool UpdateChildren()
  {
    InvalidateChildren();
    return EnsureChildren();
  }

  /**
   * Cache children if necessary. Return true if the accessible is defunct.
   */
  bool EnsureChildren();

  /**
   * Set the child count to -1 (unknown) and null out cached child pointers.
   * Should be called when accessible tree is changed because document has
   * transformed. Note, if accessible cares about its parent relation chain
   * itself should override this method to do nothing.
   */
  virtual void InvalidateChildren();

  /**
   * Append/insert/remove a child. Return true if operation was successful.
   */
  virtual bool AppendChild(nsAccessible* aChild);
  virtual bool InsertChildAt(PRUint32 aIndex, nsAccessible* aChild);
  virtual bool RemoveChild(nsAccessible* aChild);

  //////////////////////////////////////////////////////////////////////////////
  // Accessible tree traverse methods

  /**
   * Return parent accessible.
   */
  nsAccessible* Parent() const { return mParent; }

  /**
   * Return child accessible at the given index.
   */
  virtual nsAccessible* GetChildAt(PRUint32 aIndex);

  /**
   * Return child accessible count.
   */
  virtual PRInt32 GetChildCount();

  /**
   * Return index of the given child accessible.
   */
  virtual PRInt32 GetIndexOf(nsAccessible* aChild);

  /**
   * Return index in parent accessible.
   */
  virtual PRInt32 IndexInParent() const;

  /**
   * Return true if accessible has children;
   */
  bool HasChildren() { return !!GetChildAt(0); }

  /**
   * Return first/last/next/previous sibling of the accessible.
   */
  inline nsAccessible* NextSibling() const
    {  return GetSiblingAtOffset(1); }
  inline nsAccessible* PrevSibling() const
    { return GetSiblingAtOffset(-1); }
  inline nsAccessible* FirstChild()
    { return GetChildCount() != 0 ? GetChildAt(0) : nsnull; }
  inline nsAccessible* LastChild()
  {
    PRUint32 childCount = GetChildCount();
    return childCount != 0 ? GetChildAt(childCount - 1) : nsnull;
  }


  /**
   * Return embedded accessible children count.
   */
  PRInt32 GetEmbeddedChildCount();

  /**
   * Return embedded accessible child at the given index.
   */
  nsAccessible* GetEmbeddedChildAt(PRUint32 aIndex);

  /**
   * Return index of the given embedded accessible child.
   */
  PRInt32 GetIndexOfEmbeddedChild(nsAccessible* aChild);

  /**
   * Return number of content children/content child at index. The content
   * child is created from markup in contrast to it's never constructed by its
   * parent accessible (like treeitem accessibles for XUL trees).
   */
  PRUint32 ContentChildCount() const { return mChildren.Length(); }
  nsAccessible* ContentChildAt(PRUint32 aIndex) const
    { return mChildren.ElementAt(aIndex); }

  /**
   * Return true if children were initialized.
   */
  inline bool AreChildrenCached() const
    { return !IsChildrenFlag(eChildrenUninitialized); }

  /**
   * Return true if the accessible is attached to tree.
   */
  bool IsBoundToParent() const { return !!mParent; }

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous methods

  /**
   * Handle accessible event, i.e. process it, notifies observers and fires
   * platform specific event.
   */
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent);

  /**
   * Return true if this accessible allows accessible children from anonymous subtree.
   */
  virtual bool CanHaveAnonChildren();

  /**
   * Returns text of accessible if accessible has text role otherwise empty
   * string.
   *
   * @param aText         [in] returned text of the accessible
   * @param aStartOffset  [in, optional] start offset inside of the accessible,
   *                        if missed entire text is appended
   * @param aLength       [in, optional] required length of text, if missed
   *                        then text form start offset till the end is appended
   */
  virtual void AppendTextTo(nsAString& aText, PRUint32 aStartOffset = 0,
                            PRUint32 aLength = PR_UINT32_MAX);

  /**
   * Assert if child not in parent's cache if the cache was initialized at this
   * point.
   */
  void TestChildCache(nsAccessible* aCachedChild) const;

  //////////////////////////////////////////////////////////////////////////////
  // Downcasting and types

  inline bool IsAbbreviation() const
  {
    return mContent->IsHTML() &&
      (mContent->Tag() == nsGkAtoms::abbr || mContent->Tag() == nsGkAtoms::acronym);
  }

  inline bool IsApplication() const { return mFlags & eApplicationAccessible; }

  bool IsAutoComplete() const { return mFlags & eAutoCompleteAccessible; }

  inline bool IsAutoCompletePopup() const { return mFlags & eAutoCompletePopupAccessible; }

  inline bool IsCombobox() const { return mFlags & eComboboxAccessible; }

  inline bool IsDoc() const { return mFlags & eDocAccessible; }
  nsDocAccessible* AsDoc();

  inline bool IsHyperText() const { return mFlags & eHyperTextAccessible; }
  nsHyperTextAccessible* AsHyperText();

  inline bool IsHTMLFileInput() const { return mFlags & eHTMLFileInputAccessible; }

  inline bool IsHTMLListItem() const { return mFlags & eHTMLListItemAccessible; }
  nsHTMLLIAccessible* AsHTMLListItem();

  inline bool IsImageAccessible() const { return mFlags & eImageAccessible; }
  nsHTMLImageAccessible* AsImage();

  bool IsImageMapAccessible() const { return mFlags & eImageMapAccessible; }
  nsHTMLImageMapAccessible* AsImageMap();

  inline bool IsListControl() const { return mFlags & eListControlAccessible; }

  inline bool IsMenuButton() const { return mFlags & eMenuButtonAccessible; }

  inline bool IsMenuPopup() const { return mFlags & eMenuPopupAccessible; }

  inline bool IsRoot() const { return mFlags & eRootAccessible; }
  nsRootAccessible* AsRoot();

  inline bool IsTextLeaf() const { return mFlags & eTextLeafAccessible; }
  nsTextAccessible* AsTextLeaf();

  //////////////////////////////////////////////////////////////////////////////
  // ActionAccessible

  /**
   * Return the number of actions that can be performed on this accessible.
   */
  virtual PRUint8 ActionCount();

  /**
   * Return access key, such as Alt+D.
   */
  virtual KeyBinding AccessKey() const;

  /**
   * Return global keyboard shortcut for default action, such as Ctrl+O for
   * Open file menuitem.
   */
  virtual KeyBinding KeyboardShortcut() const;

  //////////////////////////////////////////////////////////////////////////////
  // HyperLinkAccessible

  /**
   * Return true if the accessible is hyper link accessible.
   */
  virtual bool IsLink();

  /**
   * Return the start offset of the link within the parent accessible.
   */
  virtual PRUint32 StartOffset();

  /**
   * Return the end offset of the link within the parent accessible.
   */
  virtual PRUint32 EndOffset();

  /**
   * Return true if the link is valid (e. g. points to a valid URL).
   */
  inline bool IsLinkValid()
  {
    NS_PRECONDITION(IsLink(), "IsLinkValid is called on not hyper link!");

    // XXX In order to implement this we would need to follow every link
    // Perhaps we can get information about invalid links from the cache
    // In the mean time authors can use role="link" aria-invalid="true"
    // to force it for links they internally know to be invalid
    return (0 == (State() & mozilla::a11y::states::INVALID));
  }

  /**
   * Return true if the link currently has the focus.
   */
  bool IsLinkSelected();

  /**
   * Return the number of anchors within the link.
   */
  virtual PRUint32 AnchorCount();

  /**
   * Returns an anchor accessible at the given index.
   */
  virtual nsAccessible* AnchorAt(PRUint32 aAnchorIndex);

  /**
   * Returns an anchor URI at the given index.
   */
  virtual already_AddRefed<nsIURI> AnchorURIAt(PRUint32 aAnchorIndex);

  //////////////////////////////////////////////////////////////////////////////
  // SelectAccessible

  /**
   * Return true if the accessible is a select control containing selectable
   * items.
   */
  virtual bool IsSelect();

  /**
   * Return an array of selected items.
   */
  virtual already_AddRefed<nsIArray> SelectedItems();

  /**
   * Return the number of selected items.
   */
  virtual PRUint32 SelectedItemCount();

  /**
   * Return selected item at the given index.
   */
  virtual nsAccessible* GetSelectedItem(PRUint32 aIndex);

  /**
   * Determine if item at the given index is selected.
   */
  virtual bool IsItemSelected(PRUint32 aIndex);

  /**
   * Add item at the given index the selection. Return true if success.
   */
  virtual bool AddItemToSelection(PRUint32 aIndex);

  /**
   * Remove item at the given index from the selection. Return if success.
   */
  virtual bool RemoveItemFromSelection(PRUint32 aIndex);

  /**
   * Select all items. Return true if success.
   */
  virtual bool SelectAll();

  /**
   * Unselect all items. Return true if success.
   */
  virtual bool UnselectAll();

  //////////////////////////////////////////////////////////////////////////////
  // Widgets

  /**
   * Return true if accessible is a widget, i.e. control or accessible that
   * manages its items. Note, being a widget the accessible may be a part of
   * composite widget.
   */
  virtual bool IsWidget() const;

  /**
   * Return true if the widget is active, i.e. has a focus within it.
   */
  virtual bool IsActiveWidget() const;

  /**
   * Return true if the widget has items and items are operable by user and
   * can be activated.
   */
  virtual bool AreItemsOperable() const;

  /**
   * Return the current item of the widget, i.e. an item that has or will have
   * keyboard focus when widget gets active.
   */
  virtual nsAccessible* CurrentItem();

  /**
   * Set the current item of the widget.
   */
  virtual void SetCurrentItem(nsAccessible* aItem);

  /**
   * Return container widget this accessible belongs to.
   */
  virtual nsAccessible* ContainerWidget() const;

  /**
   * Return the localized string for the given key.
   */
  static void TranslateString(const nsAString& aKey, nsAString& aStringOut);

protected:

  //////////////////////////////////////////////////////////////////////////////
  // Initializing, cache and tree traverse methods

  /**
   * Cache accessible children.
   */
  virtual void CacheChildren();

  /**
   * Set accessible parent and index in parent.
   */
  virtual void BindToParent(nsAccessible* aParent, PRUint32 aIndexInParent);
  virtual void UnbindFromParent();

  /**
   * Return sibling accessible at the given offset.
   */
  virtual nsAccessible* GetSiblingAtOffset(PRInt32 aOffset,
                                           nsresult *aError = nsnull) const;

  /**
   * Flags used to describe the state and type of children.
   */
  enum ChildrenFlags {
    eChildrenUninitialized = 0, // children aren't initialized
    eMixedChildren = 1 << 0, // text leaf children are presented
    eEmbeddedChildren = 1 << 1 // all children are embedded objects
  };

  /**
   * Return true if the children flag is set.
   */
  inline bool IsChildrenFlag(ChildrenFlags aFlag) const
    { return static_cast<ChildrenFlags> (mFlags & kChildrenFlagsMask) == aFlag; }

  /**
   * Set children flag.
   */
  inline void SetChildrenFlag(ChildrenFlags aFlag)
    { mFlags = (mFlags & ~kChildrenFlagsMask) | aFlag; }

  /**
   * Flags describing the accessible itself.
   * @note keep these flags in sync with ChildrenFlags
   */
  enum AccessibleTypes {
    eApplicationAccessible = 1 << 2,
    eAutoCompleteAccessible = 1 << 3,
    eAutoCompletePopupAccessible = 1 << 4,
    eComboboxAccessible = 1 << 5,
    eDocAccessible = 1 << 6,
    eHyperTextAccessible = 1 << 7,
    eHTMLFileInputAccessible = 1 << 8,
    eHTMLListItemAccessible = 1 << 9,
    eImageAccessible = 1 << 10,
    eImageMapAccessible = 1 << 11,
    eListControlAccessible = 1 << 12,
    eMenuButtonAccessible = 1 << 13,
    eMenuPopupAccessible = 1 << 14,
    eRootAccessible = 1 << 15,
    eTextLeafAccessible = 1 << 16
  };

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous helpers

  /**
   * Return ARIA role (helper method).
   */
  mozilla::a11y::role ARIARoleInternal();

  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);

  //////////////////////////////////////////////////////////////////////////////
  // Name helpers

  /**
   * Compute the name of HTML node.
   */
  nsresult GetHTMLName(nsAString& aName);

  /**
   * Compute the name for XUL node.
   */
  nsresult GetXULName(nsAString& aName);

  // helper method to verify frames
  static nsresult GetFullKeyName(const nsAString& aModifierName, const nsAString& aKeyName, nsAString& aStringOut);

  /**
   * Return an accessible for the given DOM node, or if that node isn't
   * accessible, return the accessible for the next DOM node which has one
   * (based on forward depth first search).
   *
   * @param  aStartNode  [in] the DOM node to start from
   * @return              the resulting accessible
   */
  nsAccessible *GetFirstAvailableAccessible(nsINode *aStartNode) const;

  //////////////////////////////////////////////////////////////////////////////
  // Action helpers

  /**
   * Prepares click action that will be invoked in timeout.
   *
   * @note  DoCommand() prepares an action in timeout because when action
   *  command opens a modal dialog/window, it won't return until the
   *  dialog/window is closed. If executing action command directly in
   *  nsIAccessible::DoAction() method, it will block AT tools (e.g. GOK) that
   *  invoke action of mozilla accessibles direclty (see bug 277888 for details).
   *
   * @param  aContent      [in, optional] element to click
   * @param  aActionIndex  [in, optional] index of accessible action
   */
  void DoCommand(nsIContent *aContent = nsnull, PRUint32 aActionIndex = 0);

  /**
   * Dispatch click event.
   */
  virtual void DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex);

  NS_DECL_RUNNABLEMETHOD_ARG2(nsAccessible, DispatchClickEvent,
                              nsCOMPtr<nsIContent>, PRUint32)

  //////////////////////////////////////////////////////////////////////////////
  // Helpers

  /**
   *  Get the container node for an atomic region, defined by aria-atomic="true"
   *  @return the container node
   */
  nsIContent* GetAtomicRegion() const;

  /**
   * Get numeric value of the given ARIA attribute.
   *
   * @param aAriaProperty - the ARIA property we're using
   * @param aValue - value of the attribute
   *
   * @return - NS_OK_NO_ARIA_VALUE if there is no setted ARIA attribute
   */
  nsresult GetAttrValue(nsIAtom *aAriaProperty, double *aValue);

  /**
   * Return the action rule based on ARIA enum constants EActionRule
   * (see nsARIAMap.h). Used by ActionCount() and GetActionName().
   *
   * @param aStates  [in] states of the accessible
   */
  PRUint32 GetActionRule(PRUint64 aStates);

  /**
   * Return group info.
   */
  AccGroupInfo* GetGroupInfo();

  /**
   * Fires platform accessible event. It's notification method only. It does
   * change nothing on Gecko side. Don't use it until you're sure what you do
   * (see example in XUL tree accessible), use nsEventShell::FireEvent()
   * instead. MUST be overridden in wrap classes.
   *
   * @param aEvent  the accessible event to fire.
   */
  virtual nsresult FirePlatformEvent(AccEvent* aEvent) = 0;

  // Data Members
  nsRefPtr<nsAccessible> mParent;
  nsTArray<nsRefPtr<nsAccessible> > mChildren;
  PRInt32 mIndexInParent;

  static const PRUint32 kChildrenFlagsMask =
    eChildrenUninitialized | eMixedChildren | eEmbeddedChildren;

  PRUint32 mFlags;

  nsAutoPtr<EmbeddedObjCollector> mEmbeddedObjCollector;
  PRInt32 mIndexOfEmbeddedChild;
  friend class EmbeddedObjCollector;

  nsAutoPtr<AccGroupInfo> mGroupInfo;
  friend class AccGroupInfo;

  nsRoleMapEntry *mRoleMapEntry; // Non-null indicates author-supplied role; possibly state & value as well
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccessible,
                              NS_ACCESSIBLE_IMPL_IID)


/**
 * Represent key binding associated with accessible (such as access key and
 * global keyboard shortcuts).
 */
class KeyBinding
{
public:
  /**
   * Modifier mask values.
   */
  static const PRUint32 kShift = 1;
  static const PRUint32 kControl = 2;
  static const PRUint32 kAlt = 4;
  static const PRUint32 kMeta = 8;

  KeyBinding() : mKey(0), mModifierMask(0) {}
  KeyBinding(PRUint32 aKey, PRUint32 aModifierMask) :
    mKey(aKey), mModifierMask(aModifierMask) {};

  inline bool IsEmpty() const { return !mKey; }
  inline PRUint32 Key() const { return mKey; }
  inline PRUint32 ModifierMask() const { return mModifierMask; }

  enum Format {
    ePlatformFormat,
    eAtkFormat
  };

  /**
   * Return formatted string for this key binding depending on the given format.
   */
  inline void ToString(nsAString& aValue,
                       Format aFormat = ePlatformFormat) const
  {
    aValue.Truncate();
    AppendToString(aValue, aFormat);
  }
  inline void AppendToString(nsAString& aValue,
                             Format aFormat = ePlatformFormat) const
  {
    if (mKey) {
      if (aFormat == ePlatformFormat)
        ToPlatformFormat(aValue);
      else
        ToAtkFormat(aValue);
    }
  }

private:
  void ToPlatformFormat(nsAString& aValue) const;
  void ToAtkFormat(nsAString& aValue) const;

  PRUint32 mKey;
  PRUint32 mModifierMask;
};

#endif
