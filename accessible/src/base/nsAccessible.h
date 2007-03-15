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

#include "nsIAccessibleRole.h"
#include "nsIAccessibleStates.h"
#include "nsAccessNodeWrap.h"
#include "nsAccessibilityAtoms.h"
#include "nsIAccessible.h"
#include "nsPIAccessible.h"
#include "nsIAccessibleHyperLink.h"
#include "nsIAccessibleSelectable.h"
#include "nsIAccessibleValue.h"
#include "nsIDOMNodeList.h"
#include "nsINameSpaceManager.h"
#include "nsWeakReference.h"
#include "nsString.h"
#include "nsIDOMDOMStringList.h"

struct nsRect;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsIDOMNode;
class nsIAtom;

// When mNextSibling is set to this, it indicates there ar eno more siblings
#define DEAD_END_ACCESSIBLE NS_STATIC_CAST(nsIAccessible*, (void*)1)

// Saves a data member -- if child count equals this value we haven't
// cached children or child count yet
enum { eChildCountUninitialized = -1 };

struct nsStateMapEntry
{
  const char* attributeName;  // magic value of nsnull means last entry in map
  const char* attributeValue; // magic value of nsnull means any value
  PRUint32 state;       // OR state with this
};

enum ENameRule {
  eNameLabelOrTitle,     // Collect name if explicitly specified from 
                         // 1) content subtree pointed to by labelledby
                         //    which contains the ID for the label content, or
                         // 2) title attribute if specified
  eNameOkFromChildren    // Collect name from
                         // 1) labelledby attribute if specified, or
                         // 2) text & img descendents, or
                         // 3) title attribute if specified
};

enum EValueRule {
  eNoValue,
  eHasValueMinMax    // Supports value, min and max from waistate:valuenow, valuemin and valuemax
};

#define eNoReqStates 0
#define END_ENTRY {0, 0, 0}  // To fill in array of state mappings
#define BOOL_STATE 0

struct nsRoleMapEntry
{
  const char *roleString; // such as "button"
  PRUint32 role;   // use this role
  ENameRule nameRule;  // how to compute name
  EValueRule valueRule;  // how to compute name
  PRUint32 state;  // always OR state with this
  // For this role with a DOM attribute/value match definined in
  // nsStateMapEntry.attributeName && .attributeValue, OR accessible state with
  // nsStateMapEntry.state
  // Currently you can have up to 3 DOM attributes with accessible state mappings.
  // A variable sized array would not allow use of C++'s struct initialization feature.
  nsStateMapEntry attributeMap1;
  nsStateMapEntry attributeMap2;
  nsStateMapEntry attributeMap3;
  nsStateMapEntry attributeMap4;
  nsStateMapEntry attributeMap5;
  nsStateMapEntry attributeMap6;
  nsStateMapEntry attributeMap7;
};


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
  NS_IMETHOD Init();
  NS_IMETHOD Shutdown();

  NS_IMETHOD GetState(PRUint32 *aState);  // Must support GetFinalState()

#ifdef MOZ_ACCESSIBILITY_ATK
  static PRBool FindTextFrame(PRInt32 &index, nsPresContext *aPresContext, nsIFrame *aCurFrame, 
                                   nsIFrame **aFirstTextFrame, const nsIFrame *aTextFrame);
#endif

#ifdef DEBUG_A11Y
  static PRBool IsTextInterfaceSupportCorrect(nsIAccessible *aAccessible);
#endif

  static PRBool IsCorrectFrameType(nsIFrame* aFrame, nsIAtom* aAtom);
  static PRUint32 State(nsIAccessible *aAcc) { PRUint32 state; aAcc->GetFinalState(&state); return state; }
  static PRUint32 Role(nsIAccessible *aAcc) { PRUint32 role; aAcc->GetFinalRole(&role); return role; }
  static PRBool IsText(nsIAccessible *aAcc) { PRUint32 role = Role(aAcc); return role == nsIAccessibleRole::ROLE_TEXT_LEAF || role == nsIAccessibleRole::ROLE_STATICTEXT; }
  static PRBool IsEmbeddedObject(nsIAccessible *aAcc) { PRUint32 role = Role(aAcc); return role != nsIAccessibleRole::ROLE_TEXT_LEAF && role != nsIAccessibleRole::ROLE_WHITESPACE && role != nsIAccessibleRole::ROLE_STATICTEXT; }
  static PRInt32 TextLength(nsIAccessible *aAccessible);
  static PRBool IsLeaf(nsIAccessible *aAcc) { PRInt32 numChildren; aAcc->GetChildCount(&numChildren); return numChildren > 0; }
  
  already_AddRefed<nsIAccessible> GetParent() {
    nsIAccessible *parent = nsnull;
    GetParent(&parent);
    return parent;
  }

protected:
  PRBool MappedAttrState(nsIContent *aContent, PRUint32 *aStateInOut, nsStateMapEntry *aStateMapEntry);
  virtual nsIFrame* GetBoundsFrame();
  virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
  PRBool IsVisible(PRBool *aIsOffscreen); 
  nsresult GetTextFromRelationID(nsIAtom *aIDAttrib, nsString &aName);

  static nsIContent *GetContentPointingTo(const nsAString *aId,
                                          nsIContent *aLookContent,
                                          nsIAtom *forAttrib,
                                          nsIContent *aExcludeContent = nsnull,
                                          PRUint32 aForAttribNamespace = kNameSpaceID_None,
                                          nsIAtom *aTagType = nsAccessibilityAtoms::label);
  static nsIContent *GetXULLabelContent(nsIContent *aForNode,
                                        nsIAtom *aLabelType = nsAccessibilityAtoms::label);
  static nsIContent *GetHTMLLabelContent(nsIContent *aForNode);
  static nsIContent *GetLabelContent(nsIContent *aForNode);
  static nsIContent *GetRoleContent(nsIDOMNode *aDOMNode);

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

  // Relation helpers
  already_AddRefed<nsIDOMNode> GetInverseRelatedNode(nsIAtom *aRelationAttr,
                                                     PRUint32 aAncestorLevelsToSearch = 0);

  // Data Members
  nsCOMPtr<nsIAccessible> mParent;
  nsIAccessible *mFirstChild, *mNextSibling;
  nsRoleMapEntry *mRoleMapEntry; // Non-null indicates author-supplied role; possibly state & value as well
  PRInt32 mAccChildCount;

  static nsRoleMapEntry gWAIRoleMap[];
  static nsStateMapEntry gUnivStateMap[];
};


#endif  

