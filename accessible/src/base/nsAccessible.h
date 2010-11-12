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

#include "nsAccessNodeWrap.h"

#include "nsIAccessible.h"
#include "nsIAccessibleHyperLink.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleValue.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleStates.h"

#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nsRefPtrHashtable.h"
#include "nsDataHashtable.h"

class AccGroupInfo;
class EmbeddedObjCollector;
class nsAccessible;
class AccEvent;
struct nsRoleMapEntry;

struct nsRect;
class nsIContent;
class nsIFrame;
class nsIAtom;
class nsIView;

typedef nsRefPtrHashtable<nsVoidPtrHashKey, nsAccessible>
  nsAccessibleHashtable;
typedef nsDataHashtable<nsPtrHashKey<const nsINode>, nsAccessible*>
  NodeToAccessibleMap;

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
  nsAccessible(nsIContent *aContent, nsIWeakReference *aShell);
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
   * Returns the accessible name specified by ARIA.
   */
  nsresult GetARIAName(nsAString& aName);

  /**
   * Maps ARIA state attributes to state of accessible. Note the given state
   * argument should hold states for accessible before you pass it into this
   * method.
   *
   * @param  [in/out] where to fill the states into.
   * @param  [in/out] where to fill the extra states into
   */
  virtual nsresult GetARIAState(PRUint32 *aState, PRUint32 *aExtraState);

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
   * Return enumerated accessible role (see constants in nsIAccessibleRole).
   */
  virtual PRUint32 Role();

  /**
   * Returns enumerated accessible role from native markup (see constants in
   * nsIAccessibleRole). Doesn't take into account ARIA roles.
   */
  virtual PRUint32 NativeRole();

  /**
   * Return the state of accessible that doesn't take into account ARIA states.
   * Use nsIAccessible::state to get all states for accessible. If
   * second argument is omitted then second bit field of accessible state won't
   * be calculated.
   */
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  /**
   * Returns attributes for accessible without explicitly setted ARIA
   * attributes.
   */
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

  /**
   * Return direct or deepest child at the given point.
   *
   * @param  aX             [in] x coordinate relative screen
   * @param  aY             [in] y coordinate relative screen
   * @param  aDeepestChild  [in] flag points if deep child should be returned
   * @param  aChild         [out] found child
   */
  virtual nsresult GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   PRBool aDeepestChild,
                                   nsIAccessible **aChild);

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
  const nsRoleMapEntry* GetRoleMapEntry() const { return mRoleMapEntry; }

  /**
   * Cache children if necessary. Return true if the accessible is defunct.
   */
  PRBool EnsureChildren();

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
  virtual PRBool AppendChild(nsAccessible* aChild);
  virtual PRBool InsertChildAt(PRUint32 aIndex, nsAccessible* aChild);
  virtual PRBool RemoveChild(nsAccessible* aChild);

  //////////////////////////////////////////////////////////////////////////////
  // Accessible tree traverse methods

  /**
   * Return parent accessible.
   */
  nsAccessible* GetParent();

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
  virtual PRInt32 GetIndexInParent();

  /**
   * Return true if accessible has children;
   */
  PRBool HasChildren() { return !!GetChildAt(0); }

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
   * Return cached accessible of parent-child relatives.
   */
  nsAccessible* GetCachedParent() const { return mParent; }
  nsAccessible* GetCachedNextSibling() const
  {
    return mParent ?
      mParent->mChildren.SafeElementAt(mIndexInParent + 1, nsnull).get() : nsnull;
  }
  nsAccessible* GetCachedPrevSibling() const
  {
    return mParent ?
      mParent->mChildren.SafeElementAt(mIndexInParent - 1, nsnull).get() : nsnull;
  }
  PRUint32 GetCachedChildCount() const { return mChildren.Length(); }
  nsAccessible* GetCachedChildAt(PRUint32 aIndex) const { return mChildren.ElementAt(aIndex); }
  PRBool AreChildrenCached() const { return mChildrenFlags != eChildrenUninitialized; }
  bool IsBoundToParent() const { return mParent; }

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous methods

  /**
   * Handle accessible event, i.e. process it, notifies observers and fires
   * platform specific event.
   */
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent);

  /**
   * Return true if there are accessible children in anonymous content
   */
  virtual PRBool GetAllowsAnonChildAccessibles();

  /**
   * Returns text of accessible if accessible has text role otherwise empty
   * string.
   *
   * @param aText         returned text of the accessible
   * @param aStartOffset  start offset inside of the accesible
   * @param aLength       required lenght of text
   */
  virtual nsresult AppendTextTo(nsAString& aText, PRUint32 aStartOffset,
                                PRUint32 aLength);

  /**
   * Assert if child not in parent's cache if the cache was initialized at this
   * point.
   */
  void TestChildCache(nsAccessible *aCachedChild);

  //////////////////////////////////////////////////////////////////////////////
  // HyperLinkAccessible

  /**
   * Return true if the accessible is hyper link accessible.
   */
  virtual bool IsHyperLink();

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
  virtual bool IsValid();

  /**
   * Return true if the link currently has the focus.
   */
  virtual bool IsSelected();

  /**
   * Return the number of anchors within the link.
   */
  virtual PRUint32 AnchorCount();

  /**
   * Returns an anchor accessible at the given index.
   */
  virtual nsAccessible* GetAnchor(PRUint32 aAnchorIndex);

  /**
   * Returns an anchor URI at the given index.
   */
  virtual already_AddRefed<nsIURI> GetAnchorURI(PRUint32 aAnchorIndex);

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
  void BindToParent(nsAccessible* aParent, PRUint32 aIndexInParent);
  void UnbindFromParent();

  /**
   * Return sibling accessible at the given offset.
   */
  virtual nsAccessible* GetSiblingAtOffset(PRInt32 aOffset,
                                           nsresult *aError = nsnull);

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous helpers

  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
  PRBool IsVisible(PRBool *aIsOffscreen); 

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
  static nsresult GetTranslatedString(const nsAString& aKey, nsAString& aStringOut);

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

  // Check the visibility across both parent content and chrome
  PRBool CheckVisibilityInParentChain(nsIDocument* aDocument, nsIView* aView);

  /**
   *  Get the container node for an atomic region, defined by aria-atomic="true"
   *  @return the container node
   */
  nsIDOMNode* GetAtomicRegion();

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
   * (see nsARIAMap.h). Used by GetNumActions() and GetActionName().
   *
   * @param aStates  [in] states of the accessible
   */
  PRUint32 GetActionRule(PRUint32 aStates);

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

  enum ChildrenFlags {
    eChildrenUninitialized = 0x00,
    eMixedChildren = 0x01,
    eEmbeddedChildren = 0x02
  };
  ChildrenFlags mChildrenFlags;

  nsAutoPtr<EmbeddedObjCollector> mEmbeddedObjCollector;
  PRInt32 mIndexOfEmbeddedChild;
  friend class EmbeddedObjCollector;

  nsAutoPtr<AccGroupInfo> mGroupInfo;
  friend class AccGroupInfo;

  nsRoleMapEntry *mRoleMapEntry; // Non-null indicates author-supplied role; possibly state & value as well
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccessible,
                              NS_ACCESSIBLE_IMPL_IID)

#endif
