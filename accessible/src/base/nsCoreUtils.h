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


#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIBoxObject.h"
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"

#include "nsIFrame.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMDOMStringList.h"
#include "nsIMutableArray.h"
#include "nsPoint.h"
#include "nsTArray.h"

/**
 * Core utils.
 */
class nsCoreUtils
{
public:
  /**
   * Return true if the given node has registered click, mousedown or mouseup
   * event listeners.
   */
  static bool HasClickListener(nsIContent *aContent);

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
  static bool DispatchMouseEvent(PRUint32 aEventType,
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
   * nsEventStateManager or 0 if there is no registered accesskey.
   *
   * @param aContent - the given element.
   */
  static PRUint32 GetAccessKeyFor(nsIContent *aContent);

  /**
   * Return DOM element related with the given node, i.e.
   * a) itself if it is DOM element
   * b) parent element if it is text node
   * c) otherwise nsnull
   *
   * @param aNode  [in] the given DOM node
   */
  static nsIContent* GetDOMElementFor(nsIContent *aContent);

  /**
   * Return DOM node for the given DOM point.
   */
  static nsINode *GetDOMNodeFromDOMPoint(nsINode *aNode, PRUint32 aOffset);

  /**
   * Return the nsIContent* to check for ARIA attributes on -- this may not
   * always be the DOM node for the accessible. Specifically, for doc
   * accessibles, it is not the document node, but either the root element or
   * <body> in HTML.
   *
   * @param aNode  [in] DOM node for the accessible that may be affected by ARIA
   * @return        the nsIContent which may have ARIA markup
   */
  static nsIContent* GetRoleContent(nsINode *aNode);

  /**
   * Is the first passed in node an ancestor of the second?
   * Note: A node is not considered to be the ancestor of itself.
   *
   * @param  aPossibleAncestorNode   [in] node to test for ancestor-ness of
   *                                   aPossibleDescendantNode
   * @param  aPossibleDescendantNode [in] node to test for descendant-ness of
   *                                   aPossibleAncestorNode
   * @param  aRootNode               [in, optional] the root node that search
   *                                   search should be performed within
   * @return true                     if aPossibleAncestorNode is an ancestor of
   *                                   aPossibleDescendantNode
   */
   static bool IsAncestorOf(nsINode *aPossibleAncestorNode,
                              nsINode *aPossibleDescendantNode,
                              nsINode *aRootNode = nsnull);

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
  static nsIntPoint GetScreenCoordsForWindow(nsINode *aNode);

  /**
   * Return document shell tree item for the given DOM node.
   */
  static already_AddRefed<nsIDocShellTreeItem>
    GetDocShellTreeItemFor(nsINode *aNode);

  /**
   * Return true if the given document is root document.
   */
  static bool IsRootDocument(nsIDocument *aDocument);

  /**
   * Return true if the given document is content document (not chrome).
   */
  static bool IsContentDocument(nsIDocument *aDocument);

  /**
   * Return true if the given document node is for tab document accessible.
   */
  static bool IsTabDocument(nsIDocument* aDocumentNode);

  /**
   * Return true if the given document is an error page.
   */
  static bool IsErrorPage(nsIDocument *aDocument);

  /**
   * Retrun true if the type of given frame equals to the given frame type.
   *
   * @param aFrame  the frame
   * @param aAtom   the frame type
   */
  static bool IsCorrectFrameType(nsIFrame* aFrame, nsIAtom* aAtom);

  /**
   * Return presShell for the document containing the given DOM node.
   */
  static nsIPresShell *GetPresShellFor(nsINode *aNode)
  {
    return aNode->OwnerDoc()->GetShell();
  }
  static already_AddRefed<nsIWeakReference> GetWeakShellFor(nsINode *aNode)
  {
    nsCOMPtr<nsIWeakReference> weakShell =
      do_GetWeakReference(GetPresShellFor(aNode));
    return weakShell.forget();
  }

  /**
   * Return document node for the given document shell tree item.
   */
  static already_AddRefed<nsIDOMNode>
    GetDOMNodeForContainer(nsIDocShellTreeItem *aContainer);

  /**
   * Get the ID for an element, in some types of XML this may not be the ID attribute
   * @param aContent  Node to get the ID for
   * @param aID       Where to put ID string
   * @return          true if there is an ID set for this node
   */
  static bool GetID(nsIContent *aContent, nsAString& aID);

  /**
   * Convert attribute value of the given node to positive integer. If no
   * attribute or wrong value then false is returned.
   */
  static bool GetUIntAttr(nsIContent *aContent, nsIAtom *aAttr,
                            PRInt32 *aUInt);

  /**
   * Check if the given element is XLink.
   *
   * @param aContent  the given element
   * @return          true if the given element is XLink
   */
  static bool IsXLink(nsIContent *aContent);

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
   * Return computed styles declaration for the given node.
   */
  static already_AddRefed<nsIDOMCSSStyleDeclaration>
    GetComputedStyleDeclaration(const nsAString& aPseudoElt,
                                nsIContent *aContent);

  /**
   * Return box object for XUL treechildren element by tree box object.
   */
  static already_AddRefed<nsIBoxObject>
    GetTreeBodyBoxObject(nsITreeBoxObject *aTreeBoxObj);

  /**
   * Return tree box object from any levels DOMNode under the XUL tree.
   */
  static already_AddRefed<nsITreeBoxObject>
    GetTreeBoxObject(nsIContent* aContent);

  /**
   * Return first sensible column for the given tree box object.
   */
  static already_AddRefed<nsITreeColumn>
    GetFirstSensibleColumn(nsITreeBoxObject *aTree);

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
  static bool IsColumnHidden(nsITreeColumn *aColumn);

  /**
   * Return true if the given node is table header element.
   */
  static bool IsHTMLTableHeader(nsIContent *aContent)
  {
    return aContent->NodeInfo()->Equals(nsGkAtoms::th) ||
      aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::scope);
  }

};


/**
 * nsIDOMDOMStringList implementation.
 */
class nsAccessibleDOMStringList : public nsIDOMDOMStringList
{
public:
  nsAccessibleDOMStringList() {};
  virtual ~nsAccessibleDOMStringList() {};

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMSTRINGLIST

  bool Add(const nsAString& aName) {
    return mNames.AppendElement(aName) != nsnull;
  }

private:
  nsTArray<nsString> mNames;
};

#endif

