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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef nsCoreUtils_h_
#define nsCoreUtils_h_

#include "nsAccessibilityAtoms.h"

#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIBoxObject.h"
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"

#include "nsIFrame.h"
#include "nsIDocShellTreeItem.h"
#include "nsIArray.h"
#include "nsIMutableArray.h"
#include "nsPoint.h"

class nsCoreUtils
{
public:
  /**
   * Return true if the given node has registered event listener of the given
   * type.
   */
  static PRBool HasListener(nsIContent *aContent, const nsAString& aEventType);

  /**
   * Dispatch click event to XUL tree cell.
   *
   * @param  aTreeBoxObj  [in] tree box object
   * @param  aRowIndex    [in] row index
   * @param  aColumn      [in] column object
   * @param  aPseudoElm   [in] pseudo elemenet inside the cell, see
   *                       nsITreeBoxObject for available values
   */
  static void DispatchClickEvent(nsITreeBoxObject *aTreeBoxObj,
                                 PRInt32 aRowIndex, nsITreeColumn *aColumn,
                                 const nsCString& aPseudoElt = EmptyCString());

  /**
   * Send mouse event to the given element.
   *
   * @param  aEventType  [in] an event type (see nsGUIEvent.h for constants)
   * @param  aPresShell  [in] the presshell for the given element
   * @param  aContent    [in] the element
   */
  static PRBool DispatchMouseEvent(PRUint32 aEventType,
                                   nsIPresShell *aPresShell,
                                   nsIContent *aContent);

  /**
   * Send mouse event to the given element.
   *
   * @param aEventType   [in] an event type (see nsGUIEvent.h for constants)
   * @param aX           [in] x coordinate in dev pixels
   * @param aY           [in] y coordinate in dev pixels
   * @param aContent     [in] the element
   * @param aFrame       [in] frame of the element
   * @param aPresShell   [in] the presshell for the element
   * @param aRootWidget  [in] the root widget of the element
   */
  static void DispatchMouseEvent(PRUint32 aEventType, PRInt32 aX, PRInt32 aY,
                                 nsIContent *aContent, nsIFrame *aFrame,
                                 nsIPresShell *aPresShell,
                                 nsIWidget *aRootWidget);

  /**
   * Return an accesskey registered on the given element by
   * nsIEventStateManager or 0 if there is no registered accesskey.
   *
   * @param aContent - the given element.
   */
  static PRUint32 GetAccessKeyFor(nsIContent *aContent);

  /**
   * Return DOM element related with the given node, i.e.
   * a) itself if it is DOM element
   * b) parent element if it is text node
   * c) body element if it is HTML document node
   * d) document element if it is document node.
   *
   * @param aNode  [in] the given DOM node
   */
  static already_AddRefed<nsIDOMElement> GetDOMElementFor(nsIDOMNode *aNode);

  /**
   * Return DOM node for the given DOM point.
   */
  static already_AddRefed<nsIDOMNode> GetDOMNodeFromDOMPoint(nsIDOMNode *aNode,
                                                             PRUint32 aOffset);
  /**
   * Return the nsIContent* to check for ARIA attributes on -- this may not
   * always be the DOM node for the accessible. Specifically, for doc
   * accessibles, it is not the document node, but either the root element or
   * <body> in HTML. Similar with GetDOMElementFor() method.
   *
   * @param aDOMNode  DOM node for the accessible that may be affected by ARIA
   * @return          the nsIContent which may have ARIA markup
   */
  static nsIContent *GetRoleContent(nsIDOMNode *aDOMNode);

  /**
   * Is the first passed in node an ancestor of the second?
   * Note: A node is not considered to be the ancestor of itself.
   *
   * @param  aPossibleAncestorNode   [in] node to test for ancestor-ness of
   *                                   aPossibleDescendantNode
   * @param  aPossibleDescendantNode [in] node to test for descendant-ness of
   *                                   aPossibleAncestorNode
   * @return PR_TRUE                  if aPossibleAncestorNode is an ancestor of
   *                                   aPossibleDescendantNode
   */
   static PRBool IsAncestorOf(nsINode *aPossibleAncestorNode,
                              nsINode *aPossibleDescendantNode);

  /**
   * Are the first node and the second siblings?
   *
   * @return PR_TRUE if aDOMNode1 and aDOMNode2 have same parent
   */
   static PRBool AreSiblings(nsINode *aNode1, nsINode *aNode2);

  /**
   * Helper method to scroll range into view, used for implementation of
   * nsIAccessibleText::scrollSubstringTo().
   *
   * @param aFrame        the frame for accessible the range belongs to.
   * @param aStartNode    start node of a range
   * @param aStartOffset  an offset inside the start node
   * @param aEndNode      end node of a range
   * @param aEndOffset    an offset inside the end node
   * @param aScrollType   the place a range should be scrolled to
   */
  static nsresult ScrollSubstringTo(nsIFrame *aFrame,
                                    nsIDOMNode *aStartNode, PRInt32 aStartIndex,
                                    nsIDOMNode *aEndNode, PRInt32 aEndIndex,
                                    PRUint32 aScrollType);

  /** Helper method to scroll range into view, used for implementation of
   * nsIAccessibleText::scrollSubstringTo[Point]().
   *
   * @param aFrame        the frame for accessible the range belongs to.
   * @param aStartNode    start node of a range
   * @param aStartOffset  an offset inside the start node
   * @param aEndNode      end node of a range
   * @param aEndOffset    an offset inside the end node
   * @param aVPercent     how to align vertically, specified in percents
   * @param aHPercent     how to align horizontally, specified in percents
   */
  static nsresult ScrollSubstringTo(nsIFrame *aFrame,
                                    nsIDOMNode *aStartNode, PRInt32 aStartIndex,
                                    nsIDOMNode *aEndNode, PRInt32 aEndIndex,
                                    PRInt16 aVPercent, PRInt16 aHPercent);

  /**
   * Scrolls the given frame to the point, used for implememntation of
   * nsIAccessNode::scrollToPoint and nsIAccessibleText::scrollSubstringToPoint.
   *
   * @param aScrollableFrame  the scrollable frame
   * @param aFrame            the frame to scroll
   * @param aPoint            the point scroll to
   */
  static void ScrollFrameToPoint(nsIFrame *aScrollableFrame,
                                 nsIFrame *aFrame, const nsIntPoint& aPoint);

  /**
   * Converts scroll type constant defined in nsIAccessibleScrollType to
   * vertical and horizontal percents.
   */
  static void ConvertScrollTypeToPercents(PRUint32 aScrollType,
                                          PRInt16 *aVPercent,
                                          PRInt16 *aHPercent);

  /**
   * Returns coordinates relative screen for the top level window.
   *
   * @param aNode  the DOM node hosted in the window.
   */
  static nsIntPoint GetScreenCoordsForWindow(nsIDOMNode *aNode);

  /**
   * Return document shell tree item for the given DOM node.
   */
  static already_AddRefed<nsIDocShellTreeItem>
    GetDocShellTreeItemFor(nsIDOMNode *aNode);

  /**
   * Retrun frame for the given DOM element.
   */
  static nsIFrame* GetFrameFor(nsIDOMElement *aElm);

  /**
   * Retrun true if the type of given frame equals to the given frame type.
   *
   * @param aFrame  the frame
   * @param aAtom   the frame type
   */
  static PRBool IsCorrectFrameType(nsIFrame* aFrame, nsIAtom* aAtom);

  /**
   * Return presShell for the document containing the given DOM node.
   */
  static already_AddRefed<nsIPresShell> GetPresShellFor(nsIDOMNode *aNode);

  /**
   * Return document node for the given document shell tree item.
   */
  static already_AddRefed<nsIDOMNode>
    GetDOMNodeForContainer(nsIDocShellTreeItem *aContainer);

  /**
   * Get the ID for an element, in some types of XML this may not be the ID attribute
   * @param aContent  Node to get the ID for
   * @param aID       Where to put ID string
   * @return          PR_TRUE if there is an ID set for this node
   */
  static PRBool GetID(nsIContent *aContent, nsAString& aID);

  /**
   * Convert attribute value of the given node to positive integer. If no
   * attribute or wrong value then false is returned.
   */
  static PRBool GetUIntAttr(nsIContent *aContent, nsIAtom *aAttr,
                            PRInt32 *aUInt);

  /**
   * Check if the given element is XLink.
   *
   * @param aContent  the given element
   * @return          PR_TRUE if the given element is XLink
   */
  static PRBool IsXLink(nsIContent *aContent);

  /**
   * Returns language for the given node.
   *
   * @param aContent     [in] the given node
   * @param aRootContent [in] container of the given node
   * @param aLanguage    [out] language
   */
  static void GetLanguageFor(nsIContent *aContent, nsIContent *aRootContent,
                             nsAString& aLanguage);

  /**
   * Return the array of elements the given node is referred to by its
   * IDRefs attribute.
   *
   * @param aContent     [in] the given node
   * @param aAttr        [in] IDRefs attribute on the given node
   * @param aRefElements [out] result array of elements
   */
  static void GetElementsByIDRefsAttr(nsIContent *aContent, nsIAtom *aAttr,
                                      nsIArray **aRefElements);

  /**
   * Return the array of elements having IDRefs that points to the given node.
   *
   * @param  aRootContent  [in] root element to search inside
   * @param  aContent      [in] an element having ID attribute
   * @param  aIDRefsAttr   [in] IDRefs attribute
   * @param  aElements     [out] result array of elements
   */
  static void GetElementsHavingIDRefsAttr(nsIContent *aRootContent,
                                          nsIContent *aContent,
                                          nsIAtom *aIDRefsAttr,
                                          nsIArray **aElements);

  /**
   * Helper method for GetElementsHavingIDRefsAttr.
   */
  static void GetElementsHavingIDRefsAttrImpl(nsIContent *aRootContent,
                                              nsCString& aIdWithSpaces,
                                              nsIAtom *aIDRefsAttr,
                                              nsIMutableArray *aElements);

  /**
   * Return computed styles declaration for the given node.
   */
  static void GetComputedStyleDeclaration(const nsAString& aPseudoElt,
                                          nsIDOMNode *aNode,
                                          nsIDOMCSSStyleDeclaration **aCssDecl);

  /**
   * Search element in neighborhood of the given element by tag name and
   * attribute value that equals to ID attribute of the given element.
   * ID attribute can be either 'id' attribute or 'anonid' if the element is
   * anonymous.
   * The first matched content will be returned.
   *
   * @param aForNode - the given element the search is performed for
   * @param aRelationAttrs - an array of attributes, element is attribute name of searched element, ignored if aAriaProperty passed in
   * @param aAttrNum - how many attributes in aRelationAttrs
   * @param aTagName - tag name of searched element, or nsnull for any -- ignored if aAriaProperty passed in
   * @param aAncestorLevelsToSearch - points how is the neighborhood of the
   *                                  given element big.
   */
  static nsIContent *FindNeighbourPointingToNode(nsIContent *aForNode,
                                                 nsIAtom **aRelationAttrs, 
                                                 PRUint32 aAttrNum,
                                                 nsIAtom *aTagName = nsnull,
                                                 PRUint32 aAncestorLevelsToSearch = 5);

  /**
   * Overloaded version of FindNeighbourPointingToNode to accept only one
   * relation attribute.
   */
  static nsIContent *FindNeighbourPointingToNode(nsIContent *aForNode,
                                                 nsIAtom *aRelationAttr, 
                                                 nsIAtom *aTagName = nsnull,
                                                 PRUint32 aAncestorLevelsToSearch = 5);

  /**
   * Search for element that satisfies the requirements in subtree of the given
   * element. The requirements are tag name, attribute name and value of
   * attribute.
   * The first matched content will be returned.
   *
   * @param aId - value of searched attribute
   * @param aLookContent - element that search is performed inside
   * @param aRelationAttrs - an array of searched attributes
   * @param aAttrNum - how many attributes in aRelationAttrs
   * @param                 if both aAriaProperty and aRelationAttrs are null, then any element with aTagType will do
   * @param aExcludeContent - element that is skiped for search
   * @param aTagType - tag name of searched element, by default it is 'label' --
   *                   ignored if aAriaProperty passed in
   */
  static nsIContent *FindDescendantPointingToID(const nsString *aId,
                                                nsIContent *aLookContent,
                                                nsIAtom **aRelationAttrs,
                                                PRUint32 aAttrNum = 1,
                                                nsIContent *aExcludeContent = nsnull,
                                                nsIAtom *aTagType = nsAccessibilityAtoms::label);

  /**
   * Overloaded version of FindDescendantPointingToID to accept only one
   * relation attribute.
   */
  static nsIContent *FindDescendantPointingToID(const nsString *aId,
                                                nsIContent *aLookContent,
                                                nsIAtom *aRelationAttr,
                                                nsIContent *aExcludeContent = nsnull,
                                                nsIAtom *aTagType = nsAccessibilityAtoms::label);

  // Helper for FindDescendantPointingToID(), same args
  static nsIContent *FindDescendantPointingToIDImpl(nsCString& aIdWithSpaces,
                                                    nsIContent *aLookContent,
                                                    nsIAtom **aRelationAttrs,
                                                    PRUint32 aAttrNum = 1,
                                                    nsIContent *aExcludeContent = nsnull,
                                                    nsIAtom *aTagType = nsAccessibilityAtoms::label);

  /**
   * Return the label element for the given DOM element.
   */
  static nsIContent *GetLabelContent(nsIContent *aForNode);

  /**
   * Return the HTML label element for the given HTML element.
   */
  static nsIContent *GetHTMLLabelContent(nsIContent *aForNode);

  /**
   * Return box object for XUL treechildren element by tree box object.
   */
  static already_AddRefed<nsIBoxObject>
    GetTreeBodyBoxObject(nsITreeBoxObject *aTreeBoxObj);

  /**
   * Return tree box object from any levels DOMNode under the XUL tree.
   */
  static void
    GetTreeBoxObject(nsIDOMNode* aDOMNode, nsITreeBoxObject** aBoxObject);

  /**
   * Return first sensible column for the given tree box object.
   */
  static already_AddRefed<nsITreeColumn>
    GetFirstSensibleColumn(nsITreeBoxObject *aTree);

  /**
   * Return last sensible column for the given tree box object.
   */
  static already_AddRefed<nsITreeColumn>
    GetLastSensibleColumn(nsITreeBoxObject *aTree);

  /**
   * Return sensible columns count for the given tree box object.
   */
  static PRUint32 GetSensibleColumnCount(nsITreeBoxObject *aTree);

  /**
   * Return sensible column at the given index for the given tree box object.
   */
  static already_AddRefed<nsITreeColumn>
    GetSensibleColumnAt(nsITreeBoxObject *aTree, PRUint32 aIndex);

  /**
   * Return next sensible column for the given column.
   */
  static already_AddRefed<nsITreeColumn>
    GetNextSensibleColumn(nsITreeColumn *aColumn);

  /**
   * Return previous sensible column for the given column.
   */
  static already_AddRefed<nsITreeColumn>
    GetPreviousSensibleColumn(nsITreeColumn *aColumn);

  /**
   * Return true if the given column is hidden (i.e. not sensible).
   */
  static PRBool IsColumnHidden(nsITreeColumn *aColumn);

  /**
   * Return true if the given node is table header element.
   */
  static PRBool IsHTMLTableHeader(nsIContent *aContent)
  {
    return aContent->NodeInfo()->Equals(nsAccessibilityAtoms::th) ||
      aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::scope);
  }

  /**
   * Generates frames for popup subtree.
   *
   * @param aNode    [in] DOM node containing the menupopup element as a child
   * @param aIsAnon  [in] specifies whether popup should be searched inside of
   *                  anonymous or explicit content
   */
  static void GeneratePopupTree(nsIDOMNode *aNode, PRBool aIsAnon = PR_FALSE);
};

////////////////////////////////////////////////////////////////////////////////
// nsRunnable helpers
////////////////////////////////////////////////////////////////////////////////

/**
 * Use NS_DECL_RUNNABLEMETHOD_ macros to declare a runnable class for the given
 * method of the given class. There are three macros:
 *  NS_DECL_RUNNABLEMETHOD(Class, Method)
 *  NS_DECL_RUNNABLEMETHOD_ARG1(Class, Method, Arg1Type)
 *  NS_DECL_RUNNABLEMETHOD_ARG2(Class, Method, Arg1Type, Arg2Type)
 * Note Arg1Type and Arg2Type must be types which keeps the objects alive.
 *
 * Use NS_DISPATCH_RUNNABLEMETHOD_ macros to create an instance of declared
 * runnable class and dispatch it to main thread. Availabe macros are:
 *  NS_DISPATCH_RUNNABLEMETHOD(Method, Object)
 *  NS_DISPATCH_RUNNABLEMETHOD_ARG1(Method, Object, Arg1)
 *  NS_DISPATCH_RUNNABLEMETHOD_ARG2(Method, Object, Arg1, Arg2)
 */

#define NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                       \
  void Revoke()                                                                \
  {                                                                            \
    NS_IF_RELEASE(mObj);                                                       \
  }                                                                            \
                                                                               \
protected:                                                                     \
  virtual ~nsRunnableMethod_##Method()                                         \
  {                                                                            \
    NS_IF_RELEASE(mObj);                                                       \
  }                                                                            \
                                                                               \
private:                                                                       \
  ClassType *mObj;                                                             \


#define NS_DECL_RUNNABLEMETHOD(ClassType, Method)                              \
class nsRunnableMethod_##Method : public nsRunnable                            \
{                                                                              \
public:                                                                        \
  nsRunnableMethod_##Method(ClassType *aObj) : mObj(aObj)                      \
  {                                                                            \
    NS_IF_ADDREF(mObj);                                                        \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP Run()                                                          \
  {                                                                            \
    if (!mObj)                                                                 \
      return NS_OK;                                                            \
    (mObj-> Method)();                                                         \
    return NS_OK;                                                              \
  }                                                                            \
                                                                               \
  NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                             \
                                                                               \
};

#define NS_DECL_RUNNABLEMETHOD_ARG1(ClassType, Method, Arg1Type)               \
class nsRunnableMethod_##Method : public nsRunnable                            \
{                                                                              \
public:                                                                        \
  nsRunnableMethod_##Method(ClassType *aObj, Arg1Type aArg1) :                 \
    mObj(aObj), mArg1(aArg1)                                                   \
  {                                                                            \
    NS_IF_ADDREF(mObj);                                                        \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP Run()                                                          \
  {                                                                            \
    if (!mObj)                                                                 \
      return NS_OK;                                                            \
    (mObj-> Method)(mArg1);                                                    \
    return NS_OK;                                                              \
  }                                                                            \
                                                                               \
  NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                             \
  Arg1Type mArg1;                                                              \
};

#define NS_DECL_RUNNABLEMETHOD_ARG2(ClassType, Method, Arg1Type, Arg2Type)     \
class nsRunnableMethod_##Method : public nsRunnable                            \
{                                                                              \
public:                                                                        \
                                                                               \
  nsRunnableMethod_##Method(ClassType *aObj,                                   \
                            Arg1Type aArg1, Arg2Type aArg2) :                  \
    mObj(aObj), mArg1(aArg1), mArg2(aArg2)                                     \
  {                                                                            \
    NS_IF_ADDREF(mObj);                                                        \
  }                                                                            \
                                                                               \
  NS_IMETHODIMP Run()                                                          \
  {                                                                            \
    if (!mObj)                                                                 \
      return NS_OK;                                                            \
    (mObj-> Method)(mArg1, mArg2);                                             \
    return NS_OK;                                                              \
  }                                                                            \
                                                                               \
  NS_DECL_RUNNABLEMETHOD_HELPER(ClassType, Method)                             \
  Arg1Type mArg1;                                                              \
  Arg2Type mArg2;                                                              \
};

#define NS_DISPATCH_RUNNABLEMETHOD(Method, Obj)                                \
{                                                                              \
  nsCOMPtr<nsIRunnable> runnable =                                             \
    new nsRunnableMethod_##Method(Obj);                                        \
  if (runnable)                                                                \
    NS_DispatchToMainThread(runnable);                                         \
}

#define NS_DISPATCH_RUNNABLEMETHOD_ARG1(Method, Obj, Arg1)                     \
{                                                                              \
  nsCOMPtr<nsIRunnable> runnable =                                             \
    new nsRunnableMethod_##Method(Obj, Arg1);                                  \
  if (runnable)                                                                \
    NS_DispatchToMainThread(runnable);                                         \
}

#define NS_DISPATCH_RUNNABLEMETHOD_ARG2(Method, Obj, Arg1, Arg2)               \
{                                                                              \
  nsCOMPtr<nsIRunnable> runnable =                                             \
    new nsRunnableMethod_##Method(Obj, Arg1, Arg2);                            \
  if (runnable)                                                                \
    NS_DispatchToMainThread(runnable);                                         \
}

#endif

