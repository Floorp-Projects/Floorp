/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCoreUtils_h_
#define nsCoreUtils_h_


#include "nsIContent.h"
#include "nsIBoxObject.h"
#include "nsIPresShell.h"

#include "nsIDOMDOMStringList.h"
#include "nsPoint.h"
#include "nsTArray.h"

class nsRange;
class nsIFrame;
class nsIDocShell;
class nsITreeColumn;
class nsITreeBoxObject;
class nsIWidget;

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
                                 int32_t aRowIndex, nsITreeColumn *aColumn,
                                 const nsCString& aPseudoElt = EmptyCString());

  /**
   * Send mouse event to the given element.
   *
   * @param  aEventType  [in] an event type (see nsGUIEvent.h for constants)
   * @param  aPresShell  [in] the presshell for the given element
   * @param  aContent    [in] the element
   */
  static bool DispatchMouseEvent(uint32_t aEventType,
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
  static void DispatchMouseEvent(uint32_t aEventType, int32_t aX, int32_t aY,
                                 nsIContent *aContent, nsIFrame *aFrame,
                                 nsIPresShell *aPresShell,
                                 nsIWidget *aRootWidget);

  /**
   * Return an accesskey registered on the given element by
   * nsEventStateManager or 0 if there is no registered accesskey.
   *
   * @param aContent - the given element.
   */
  static uint32_t GetAccessKeyFor(nsIContent *aContent);

  /**
   * Return DOM element related with the given node, i.e.
   * a) itself if it is DOM element
   * b) parent element if it is text node
   * c) otherwise nullptr
   *
   * @param aNode  [in] the given DOM node
   */
  static nsIContent* GetDOMElementFor(nsIContent *aContent);

  /**
   * Return DOM node for the given DOM point.
   */
  static nsINode *GetDOMNodeFromDOMPoint(nsINode *aNode, uint32_t aOffset);

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
                              nsINode *aRootNode = nullptr);

  /**
   * Helper method to scroll range into view, used for implementation of
   * nsIAccessibleText::scrollSubstringTo().
   *
   * @param aFrame        the frame for accessible the range belongs to.
   * @param aRange    the range to scroll to
   * @param aScrollType   the place a range should be scrolled to
   */
  static nsresult ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                                    uint32_t aScrollType);

  /** Helper method to scroll range into view, used for implementation of
   * nsIAccessibleText::scrollSubstringTo[Point]().
   *
   * @param aFrame        the frame for accessible the range belongs to.
   * @param aRange    the range to scroll to
   * @param aVertical     how to align vertically, specified in percents, and when.
   * @param aHorizontal     how to align horizontally, specified in percents, and when.
   */
  static nsresult ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                                    nsIPresShell::ScrollAxis aVertical,
                                    nsIPresShell::ScrollAxis aHorizontal);

  /**
   * Scrolls the given frame to the point, used for implememntation of
   * nsIAccessible::scrollToPoint and nsIAccessibleText::scrollSubstringToPoint.
   *
   * @param aScrollableFrame  the scrollable frame
   * @param aFrame            the frame to scroll
   * @param aPoint            the point scroll to
   */
  static void ScrollFrameToPoint(nsIFrame *aScrollableFrame,
                                 nsIFrame *aFrame, const nsIntPoint& aPoint);

  /**
   * Converts scroll type constant defined in nsIAccessibleScrollType to
   * vertical and horizontal parameters.
   */
  static void ConvertScrollTypeToPercents(uint32_t aScrollType,
                                          nsIPresShell::ScrollAxis *aVertical,
                                          nsIPresShell::ScrollAxis *aHorizontal);

  /**
   * Returns coordinates in device pixels relative screen for the top level
   * window.
   *
   * @param aNode  the DOM node hosted in the window.
   */
  static nsIntPoint GetScreenCoordsForWindow(nsINode *aNode);

  /**
   * Return document shell for the given DOM node.
   */
  static already_AddRefed<nsIDocShell> GetDocShellFor(nsINode *aNode);

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
   * Return presShell for the document containing the given DOM node.
   */
  static nsIPresShell *GetPresShellFor(nsINode *aNode)
  {
    return aNode->OwnerDoc()->GetShell();
  }

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
                            int32_t *aUInt);

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
  static uint32_t GetSensibleColumnCount(nsITreeBoxObject *aTree);

  /**
   * Return sensible column at the given index for the given tree box object.
   */
  static already_AddRefed<nsITreeColumn>
    GetSensibleColumnAt(nsITreeBoxObject *aTree, uint32_t aIndex);

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
   * Scroll content into view.
   */
  static void ScrollTo(nsIPresShell* aPresShell, nsIContent* aContent,
                       uint32_t aScrollType);

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
  nsAccessibleDOMStringList() {}
  virtual ~nsAccessibleDOMStringList() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMSTRINGLIST

  bool Add(const nsAString& aName) {
    return mNames.AppendElement(aName) != nullptr;
  }

private:
  nsTArray<nsString> mNames;
};

#endif

