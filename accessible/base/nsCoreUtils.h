/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCoreUtils_h_
#define nsCoreUtils_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Element.h"
#include "nsIAccessibleEvent.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"  // for GetPresShell()

#include "nsPoint.h"
#include "nsTArray.h"

class nsRange;
class nsTreeColumn;
class nsIFrame;
class nsIDocShell;
class nsIWidget;

namespace mozilla {
class PresShell;
namespace dom {
class XULTreeElement;
}
}  // namespace mozilla

/**
 * Core utils.
 */
class nsCoreUtils {
 public:
  typedef mozilla::PresShell PresShell;
  typedef mozilla::dom::Document Document;

  /**
   * Return true if the given node is a label of a control.
   */
  static bool IsLabelWithControl(nsIContent* aContent);

  /**
   * Return true if the given node has registered click, mousedown or mouseup
   * event listeners.
   */
  static bool HasClickListener(nsIContent* aContent);

  /**
   * Dispatch click event to XUL tree cell.
   *
   * @param  aTree        [in] tree
   * @param  aRowIndex    [in] row index
   * @param  aColumn      [in] column object
   * @param  aPseudoElm   [in] pseudo element inside the cell, see
   *                       XULTreeElement for available values
   */
  MOZ_CAN_RUN_SCRIPT
  static void DispatchClickEvent(mozilla::dom::XULTreeElement* aTree,
                                 int32_t aRowIndex, nsTreeColumn* aColumn,
                                 const nsAString& aPseudoElt = EmptyString());

  /**
   * Send mouse event to the given element.
   *
   * @param aMessage     [in] an event message (see EventForwards.h)
   * @param aX           [in] x coordinate in dev pixels
   * @param aY           [in] y coordinate in dev pixels
   * @param aContent     [in] the element
   * @param aFrame       [in] frame of the element
   * @param aPresShell   [in] the presshell for the element
   * @param aRootWidget  [in] the root widget of the element
   */
  MOZ_CAN_RUN_SCRIPT
  static void DispatchMouseEvent(mozilla::EventMessage aMessage, int32_t aX,
                                 int32_t aY, nsIContent* aContent,
                                 nsIFrame* aFrame, PresShell* aPresShell,
                                 nsIWidget* aRootWidget);

  /**
   * Send a touch event with a single touch point to the given element.
   *
   * @param aMessage     [in] an event message (see EventForwards.h)
   * @param aX           [in] x coordinate in dev pixels
   * @param aY           [in] y coordinate in dev pixels
   * @param aContent     [in] the element
   * @param aFrame       [in] frame of the element
   * @param aPresShell   [in] the presshell for the element
   * @param aRootWidget  [in] the root widget of the element
   */
  MOZ_CAN_RUN_SCRIPT
  static void DispatchTouchEvent(mozilla::EventMessage aMessage, int32_t aX,
                                 int32_t aY, nsIContent* aContent,
                                 nsIFrame* aFrame, PresShell* aPresShell,
                                 nsIWidget* aRootWidget);

  /**
   * Return an accesskey registered on the given element by
   * EventStateManager or 0 if there is no registered accesskey.
   *
   * @param aContent - the given element.
   */
  static uint32_t GetAccessKeyFor(nsIContent* aContent);

  /**
   * Return DOM element related with the given node, i.e.
   * a) itself if it is DOM element
   * b) parent element if it is text node
   * c) otherwise nullptr
   *
   * @param aNode  [in] the given DOM node
   */
  static nsIContent* GetDOMElementFor(nsIContent* aContent);

  /**
   * Return DOM node for the given DOM point.
   */
  static nsINode* GetDOMNodeFromDOMPoint(nsINode* aNode, uint32_t aOffset);

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
  static bool IsAncestorOf(nsINode* aPossibleAncestorNode,
                           nsINode* aPossibleDescendantNode,
                           nsINode* aRootNode = nullptr);

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
   * @param aVertical     how to align vertically, specified in percents, and
   * when.
   * @param aHorizontal     how to align horizontally, specified in percents,
   * and when.
   */
  static nsresult ScrollSubstringTo(nsIFrame* aFrame, nsRange* aRange,
                                    mozilla::ScrollAxis aVertical,
                                    mozilla::ScrollAxis aHorizontal);

  /**
   * Scrolls the given frame to the point, used for implememntation of
   * nsIAccessible::scrollToPoint and nsIAccessibleText::scrollSubstringToPoint.
   *
   * @param aScrollableFrame  the scrollable frame
   * @param aFrame            the frame to scroll
   * @param aPoint            the point scroll to
   */
  static void ScrollFrameToPoint(nsIFrame* aScrollableFrame, nsIFrame* aFrame,
                                 const nsIntPoint& aPoint);

  /**
   * Converts scroll type constant defined in nsIAccessibleScrollType to
   * vertical and horizontal parameters.
   */
  static void ConvertScrollTypeToPercents(uint32_t aScrollType,
                                          mozilla::ScrollAxis* aVertical,
                                          mozilla::ScrollAxis* aHorizontal);

  /**
   * Returns coordinates in device pixels relative screen for the top level
   * window.
   *
   * @param aNode  the DOM node hosted in the window.
   */
  static nsIntPoint GetScreenCoordsForWindow(nsINode* aNode);

  /**
   * Return document shell for the given DOM node.
   */
  static already_AddRefed<nsIDocShell> GetDocShellFor(nsINode* aNode);

  /**
   * Return true if the given document is root document.
   */
  static bool IsRootDocument(Document* aDocument);

  /**
   * Return true if the given document is content document (not chrome).
   */
  static bool IsContentDocument(Document* aDocument);

  /**
   * Return true if the given document node is for tab document accessible.
   */
  static bool IsTabDocument(Document* aDocumentNode);

  /**
   * Return true if the given document is an error page.
   */
  static bool IsErrorPage(Document* aDocument);

  /**
   * Return presShell for the document containing the given DOM node.
   */
  static PresShell* GetPresShellFor(nsINode* aNode) {
    return aNode->OwnerDoc()->GetPresShell();
  }

  /**
   * Get the ID for an element, in some types of XML this may not be the ID
   * attribute
   * @param aContent  Node to get the ID for
   * @param aID       Where to put ID string
   * @return          true if there is an ID set for this node
   */
  static bool GetID(nsIContent* aContent, nsAString& aID);

  /**
   * Convert attribute value of the given node to positive integer. If no
   * attribute or wrong value then false is returned.
   */
  static bool GetUIntAttr(nsIContent* aContent, nsAtom* aAttr, int32_t* aUInt);

  /**
   * Returns language for the given node.
   *
   * @param aContent     [in] the given node
   * @param aRootContent [in] container of the given node
   * @param aLanguage    [out] language
   */
  static void GetLanguageFor(nsIContent* aContent, nsIContent* aRootContent,
                             nsAString& aLanguage);

  /**
   * Return tree from any levels DOMNode under the XUL tree.
   */
  static mozilla::dom::XULTreeElement* GetTree(nsIContent* aContent);

  /**
   * Return first sensible column for the given tree box object.
   */
  static already_AddRefed<nsTreeColumn> GetFirstSensibleColumn(
      mozilla::dom::XULTreeElement* aTree);

  /**
   * Return sensible columns count for the given tree box object.
   */
  static uint32_t GetSensibleColumnCount(mozilla::dom::XULTreeElement* aTree);

  /**
   * Return sensible column at the given index for the given tree box object.
   */
  static already_AddRefed<nsTreeColumn> GetSensibleColumnAt(
      mozilla::dom::XULTreeElement* aTree, uint32_t aIndex);

  /**
   * Return next sensible column for the given column.
   */
  static already_AddRefed<nsTreeColumn> GetNextSensibleColumn(
      nsTreeColumn* aColumn);

  /**
   * Return previous sensible column for the given column.
   */
  static already_AddRefed<nsTreeColumn> GetPreviousSensibleColumn(
      nsTreeColumn* aColumn);

  /**
   * Return true if the given column is hidden (i.e. not sensible).
   */
  static bool IsColumnHidden(nsTreeColumn* aColumn);

  /**
   * Scroll content into view.
   */
  MOZ_CAN_RUN_SCRIPT
  static void ScrollTo(PresShell* aPresShell, nsIContent* aContent,
                       uint32_t aScrollType);

  /**
   * Return true if the given node is table header element.
   */
  static bool IsHTMLTableHeader(nsIContent* aContent) {
    return aContent->NodeInfo()->Equals(nsGkAtoms::th) ||
           (aContent->IsElement() && aContent->AsElement()->HasAttr(
                                         kNameSpaceID_None, nsGkAtoms::scope));
  }

  /**
   * Returns true if the given string is empty or contains whitespace symbols
   * only. In contrast to nsWhitespaceTokenizer class it takes into account
   * non-breaking space (0xa0).
   */
  static bool IsWhitespaceString(const nsAString& aString);

  /**
   * Returns true if the given character is whitespace symbol.
   */
  static bool IsWhitespace(char16_t aChar) {
    return aChar == ' ' || aChar == '\n' || aChar == '\r' || aChar == '\t' ||
           aChar == 0xa0;
  }

  /*
   * Return true if there are any observers of accessible events.
   */
  static bool AccEventObserversExist();

  /**
   * Notify accessible event observers of an event.
   */
  static void DispatchAccEvent(RefPtr<nsIAccessibleEvent> aEvent);
};

#endif
