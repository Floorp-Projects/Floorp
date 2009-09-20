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

#include "nsARIAMap.h"
#include "nsRelUtils.h"
#include "nsTextEquivUtils.h"

#include "nsIAccessible.h"
#include "nsIAccessibleHyperLink.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleValue.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleStates.h"
#include "nsIAccessibleEvent.h"

#include "nsIDOMNodeList.h"
#include "nsINameSpaceManager.h"
#include "nsWeakReference.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIDOMDOMStringList.h"

struct nsRect;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsIDOMNode;
class nsIAtom;
class nsIView;

// see nsAccessible::GetAttrValue
#define NS_OK_NO_ARIA_VALUE \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x21)

// see nsAccessible::GetNameInternal
#define NS_OK_EMPTY_NAME \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x23)

// see nsAccessible::GetNameInternal
#define NS_OK_NAME_FROM_TOOLTIP \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x25)

// Saves a data member -- if child count equals this value we haven't
// cached children or child count yet
enum { eChildCountUninitialized = -1 };

class nsAccessibleDOMStringList : public nsIDOMDOMStringList
{
public:
  nsAccessibleDOMStringList();
  virtual ~nsAccessibleDOMStringList();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMSTRINGLIST

  PRBool Add(const nsAString& aName) {
    return mNames.AppendElement(aName) != nsnull;
  }

private:
  nsTArray<nsString> mNames;
};


#define NS_ACCESSIBLE_IMPL_CID                          \
{  /* 53cfa871-be42-47fc-b416-0033653b3151 */           \
  0x53cfa871,                                           \
  0xbe42,                                               \
  0x47fc,                                               \
  { 0xb4, 0x16, 0x00, 0x33, 0x65, 0x3b, 0x31, 0x51 }    \
}

class nsAccessible : public nsAccessNodeWrap, 
                     public nsIAccessible, 
                     public nsIAccessibleHyperLink,
                     public nsIAccessibleSelectable,
                     public nsIAccessibleValue
{
public:
  nsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual ~nsAccessible();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsAccessible, nsAccessNode)

  NS_DECL_NSIACCESSIBLE
  NS_DECL_NSIACCESSIBLEHYPERLINK
  NS_DECL_NSIACCESSIBLESELECTABLE
  NS_DECL_NSIACCESSIBLEVALUE
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCESSIBLE_IMPL_CID)

  //////////////////////////////////////////////////////////////////////////////
  // nsAccessNode

  virtual nsresult Shutdown();

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
   * Returns enumerated accessible role from native markup (see constants in
   * nsIAccessibleRole). Doesn't take into account ARIA roles.
   *
   * @param aRole  [out] accessible role.
   */
  virtual nsresult GetRoleInternal(PRUint32 *aRole);

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

  //////////////////////////////////////////////////////////////////////////////
  // Initializing and cache methods

  /**
   * Set accessible parent.
   */
  void SetParent(nsIAccessible *aParent);

  /**
   * Set first accessible child.
   */
  void SetFirstChild(nsIAccessible *aFirstChild);

  /**
   * Set next sibling accessible.
   */
  void SetNextSibling(nsIAccessible *aNextSibling);

  /**
   * Set the ARIA role map entry for a new accessible.
   * For a newly created accessible, specify which role map entry should be used.
   *
   * @param aRoleMapEntry The ARIA nsRoleMapEntry* for the accessible, or 
   *                      nsnull if none.
   */
  virtual void SetRoleMapEntry(nsRoleMapEntry *aRoleMapEntry);

  /**
   * Set the child count to -1 (unknown) and null out cached child pointers
   */
  virtual void InvalidateChildren();

  /**
   * Return parent accessible only if cached.
   */
  already_AddRefed<nsIAccessible> GetCachedParent();

  /**
   * Return first child accessible only if cached.
   */
  already_AddRefed<nsIAccessible> GetCachedFirstChild();

  /**
   * Assert if child not in parent's cache.
   */
  void TestChildCache(nsIAccessible *aCachedChild);

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous methods.

  /**
   * Fire accessible event.
   */
  virtual nsresult FireAccessibleEvent(nsIAccessibleEvent *aAccEvent);

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

  //////////////////////////////////////////////////////////////////////////////
  // Helper methods
  
  already_AddRefed<nsIAccessible> GetParent() {
    nsIAccessible *parent = nsnull;
    GetParent(&parent);
    return parent;
  }

protected:
  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
  PRBool IsVisible(PRBool *aIsOffscreen); 

  //////////////////////////////////////////////////////////////////////////////
  // Name helpers.

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
   * Walk into subtree and calculate the string which is used as the accessible
   * name or description.
   *
   * @param aContent      [in] traversed content
   * @param aFlatString   [in, out] result string
   * @param aIsRootHidden [in] specifies whether root content (we started to
   *                      traverse from) is hidden, in this case the result
   *                      string is calculated from hidden children
   *                      (this is used when hidden root content is explicitly
   *                      specified as label or description by author)
   */
  nsresult AppendFlatStringFromSubtreeRecurse(nsIContent *aContent,
                                              nsAString *aFlatString,
                                              PRBool aIsRootHidden);

  // Helpers for dealing with children
  virtual void CacheChildren();
  
  // nsCOMPtr<>& is useful here, because getter_AddRefs() nulls the comptr's value, and NextChild
  // depends on the passed-in comptr being null or already set to a child (finding the next sibling).
  nsIAccessible *NextChild(nsCOMPtr<nsIAccessible>& aAccessible);
    
  already_AddRefed<nsIAccessible> GetNextWithState(nsIAccessible *aStart, PRUint32 matchState);

  /**
   * Return an accessible for the given DOM node, or if that node isn't accessible, return the
   * accessible for the next DOM node which has one (based on forward depth first search)
   * @param aStartNode, the DOM node to start from
   * @param aRequireLeaf, only accept leaf accessible nodes
   * @return the resulting accessible
   */   
  already_AddRefed<nsIAccessible> GetFirstAvailableAccessible(nsIDOMNode *aStartNode, PRBool aRequireLeaf = PR_FALSE);

  // Hyperlink helpers
  virtual nsresult GetLinkOffset(PRInt32* aStartOffset, PRInt32* aEndOffset);

  //////////////////////////////////////////////////////////////////////////////
  // Action helpers

  /**
   * Used to describe click action target. See DoCommand() method.
   */
  struct nsCommandClosure
  {
    nsCommandClosure(nsAccessible *aAccessible, nsIContent *aContent,
                     PRUint32 aActionIndex) :
      accessible(aAccessible), content(aContent), actionIndex(aActionIndex) {}

    nsRefPtr<nsAccessible> accessible;
    nsCOMPtr<nsIContent> content;
    PRUint32 actionIndex;
  };

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
  nsresult DoCommand(nsIContent *aContent = nsnull, PRUint32 aActionIndex = 0);

  /**
   * Dispatch click event to target by calling DispatchClickEvent() method.
   *
   * @param  aTimer    [in] timer object
   * @param  aClosure  [in] nsCommandClosure object describing a target.
   */
  static void DoCommandCallback(nsITimer *aTimer, void *aClosure);

  /**
   * Dispatch click event.
   */
  virtual void DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex);

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
   * Compute group attributes ('posinset', 'setsize' and 'level') based
   * on accessible hierarchy. Used by GetAttributes() method if group attributes
   * weren't provided by ARIA or by internal accessible implementation.
   *
   * @param  aRole        [in] role of this accessible
   * @param  aAttributes  [in, out] object attributes
   */
  nsresult ComputeGroupAttributes(PRUint32 aRole,
                                  nsIPersistentProperties *aAttributes);

  /**
   * Fires platform accessible event. It's notification method only. It does
   * change nothing on Gecko side. Mostly you should use
   * nsIAccessible::FireAccessibleEvent excepting special cases like we have
   * in xul:tree accessible to lie to AT. Must be overridden in wrap classes.
   *
   * @param aEvent  the accessible event to fire.
   */
  virtual nsresult FirePlatformEvent(nsIAccessibleEvent *aEvent) = 0;

  // Data Members
  nsCOMPtr<nsIAccessible> mParent;
  nsCOMPtr<nsIAccessible> mFirstChild;
  nsCOMPtr<nsIAccessible> mNextSibling;

  nsRoleMapEntry *mRoleMapEntry; // Non-null indicates author-supplied role; possibly state & value as well
  PRInt32 mAccChildCount;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccessible,
                              NS_ACCESSIBLE_IMPL_CID)

#endif  

