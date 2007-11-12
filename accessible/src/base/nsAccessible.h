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
#include "nsAccessibilityUtils.h"

#include "nsIAccessible.h"
#include "nsPIAccessible.h"
#include "nsIAccessibleHyperLink.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleValue.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleStates.h"
#include "nsAccessibleRelationWrap.h"
#include "nsIAccessibleEvent.h"

#include "nsIDOMNodeList.h"
#include "nsINameSpaceManager.h"
#include "nsWeakReference.h"
#include "nsString.h"
#include "nsIDOMDOMStringList.h"
#include "nsARIAMap.h"

struct nsRect;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsIDOMNode;
class nsIAtom;
class nsIView;

#define NS_OK_NO_ARIA_VALUE \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x21)

// When mNextSibling is set to this, it indicates there ar eno more siblings
#define DEAD_END_ACCESSIBLE static_cast<nsIAccessible*>((void*)1)

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
    return mNames.AppendString(aName);
  }

private:
  nsStringArray mNames;
};


class nsAccessible : public nsAccessNodeWrap, 
                     public nsIAccessible, 
                     public nsPIAccessible,
                     public nsIAccessibleHyperLink,
                     public nsIAccessibleSelectable,
                     public nsIAccessibleValue
{
public:
  nsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  virtual ~nsAccessible();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLE
  NS_DECL_NSPIACCESSIBLE
  NS_DECL_NSIACCESSIBLEHYPERLINK
  NS_DECL_NSIACCESSIBLESELECTABLE
  NS_DECL_NSIACCESSIBLEVALUE

  // nsIAccessNode
  NS_IMETHOD Shutdown();

  /**
   * Return the state of accessible that doesn't take into account ARIA states.
   * Use nsIAccessible::finalState() to get all states for accessible. If
   * second argument is omitted then second bit field of accessible state won't
   * be calculated.
   */
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);

  /**
   * Returns attributes for accessible without explicitly setted ARIA
   * attributes.
   */
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

  /**
   * Maps ARIA state attributes to state of accessible. Note the given state
   * argument should hold states for accessible before you pass it into this
   * method.
   */
  PRUint32 GetARIAState();

#ifdef DEBUG_A11Y
  static PRBool IsTextInterfaceSupportCorrect(nsIAccessible *aAccessible);
#endif

  static PRBool IsCorrectFrameType(nsIFrame* aFrame, nsIAtom* aAtom);
  static PRUint32 State(nsIAccessible *aAcc) { PRUint32 state; aAcc->GetFinalState(&state, nsnull); return state; }
  static PRUint32 Role(nsIAccessible *aAcc) { PRUint32 role; aAcc->GetFinalRole(&role); return role; }
  static PRBool IsText(nsIAccessible *aAcc) { PRUint32 role = Role(aAcc); return role == nsIAccessibleRole::ROLE_TEXT_LEAF || role == nsIAccessibleRole::ROLE_STATICTEXT; }
  static PRBool IsEmbeddedObject(nsIAccessible *aAcc) { PRUint32 role = Role(aAcc); return role != nsIAccessibleRole::ROLE_TEXT_LEAF && role != nsIAccessibleRole::ROLE_WHITESPACE && role != nsIAccessibleRole::ROLE_STATICTEXT; }
  static PRInt32 TextLength(nsIAccessible *aAccessible); // Returns -1 on failure
  static PRBool IsLeaf(nsIAccessible *aAcc) { PRInt32 numChildren; aAcc->GetChildCount(&numChildren); return numChildren > 0; }
  static PRBool IsNodeRelevant(nsIDOMNode *aNode); // Is node something that could have an attached accessible
  /**
   * When exposing to platform accessibility APIs, should the children be pruned off?
   */
  static PRBool MustPrune(nsIAccessible *aAccessible);
  
  already_AddRefed<nsIAccessible> GetParent() {
    nsIAccessible *parent = nsnull;
    GetParent(&parent);
    return parent;
  }
  
  /**
   *  Return the nsIContent* to check for ARIA attributes on -- this may not always
   *  be the DOM node for the accessible. Specifically, for doc accessibles, it is not
   *  the document node, but either the root element or <body> in HTML.
   *  @param aDOMNode   The DOM node for the accessible that may be affected by ARIA
   *  @return The nsIContent which may have ARIA markup
   */
  static nsIContent *GetRoleContent(nsIDOMNode *aDOMNode);

protected:
  PRBool MappedAttrState(nsIContent *aContent, PRUint32 *aStateInOut, nsStateMapEntry *aStateMapEntry);
  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
  PRBool IsVisible(PRBool *aIsOffscreen); 

  // Relation helpers

  /**
   * For a given ARIA relation, such as labelledby or describedby, get the collated text
   * for the subtree that's pointed to.
   *
   * @param aIDProperty  The ARIA relationship property to get the text for
   * @param aName        Where to put the text
   * @return error or success code
   */
  nsresult GetTextFromRelationID(EAriaProperty aIDProperty, nsString &aName);

  static nsIContent *GetHTMLLabelContent(nsIContent *aForNode);
  static nsIContent *GetLabelContent(nsIContent *aForNode);

  // Name helpers
  nsresult GetHTMLName(nsAString& _retval, PRBool aCanAggregateSubtree = PR_TRUE);
  nsresult GetXULName(nsAString& aName, PRBool aCanAggregateSubtree = PR_TRUE);
  // For accessibles that are not lists of choices, the name of the subtree should be the 
  // sum of names in the subtree
  nsresult AppendFlatStringFromSubtree(nsIContent *aContent, nsAString *aFlatString);
  nsresult AppendNameFromAccessibleFor(nsIContent *aContent, nsAString *aFlatString,
                                       PRBool aFromValue = PR_FALSE);
  nsresult AppendFlatStringFromContentNode(nsIContent *aContent, nsAString *aFlatString);
  nsresult AppendStringWithSpaces(nsAString *aFlatString, const nsAString& textEquivalent);

  // helper method to verify frames
  static nsresult GetFullKeyName(const nsAString& aModifierName, const nsAString& aKeyName, nsAString& aStringOut);
  static nsresult GetTranslatedString(const nsAString& aKey, nsAString& aStringOut);
  nsresult AppendFlatStringFromSubtreeRecurse(nsIContent *aContent, nsAString *aFlatString);

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

  // Selection helpers
  static already_AddRefed<nsIAccessible> GetMultiSelectFor(nsIDOMNode *aNode);

  // Hyperlink helpers
  virtual nsresult GetLinkOffset(PRInt32* aStartOffset, PRInt32* aEndOffset);

  // For accessibles that have actions
  static void DoCommandCallback(nsITimer *aTimer, void *aClosure);
  nsresult DoCommand(nsIContent *aContent = nsnull);

  // Check the visibility across both parent content and chrome
  PRBool CheckVisibilityInParentChain(nsIDocument* aDocument, nsIView* aView);

  /**
   *  Get the container node for an atomic region, defined by aria:atomic="true"
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
  nsresult GetAttrValue(EAriaProperty aAriaProperty, double *aValue);

  // Data Members
  nsCOMPtr<nsIAccessible> mParent;
  nsIAccessible *mFirstChild, *mNextSibling;
  nsRoleMapEntry *mRoleMapEntry; // Non-null indicates author-supplied role; possibly state & value as well
  PRInt32 mAccChildCount;
};


#endif  

