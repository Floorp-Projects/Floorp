/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLEditUtils_h
#define mozilla_HTMLEditUtils_h

/**
 * This header declares/defines static helper methods as members of
 * HTMLEditUtils.  If you want to create or look for helper trivial classes for
 * HTMLEditor, see HTMLEditHelpers.h.
 */

#include "mozilla/Attributes.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/EnumSet.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#include "nsGkAtoms.h"
#include "nsHTMLTags.h"
#include "nsTArray.h"

class nsAtom;
class nsPresContext;

namespace mozilla {

enum class CollectChildrenOption {
  // Ignore non-editable nodes
  IgnoreNonEditableChildren,
  // Collect list children too.
  CollectListChildren,
  // Collect table children too.
  CollectTableChildren,
};

class HTMLEditUtils final {
  using AbstractRange = dom::AbstractRange;
  using Element = dom::Element;
  using Selection = dom::Selection;
  using Text = dom::Text;

 public:
  static constexpr char16_t kNewLine = '\n';
  static constexpr char16_t kCarriageReturn = '\r';
  static constexpr char16_t kTab = '\t';
  static constexpr char16_t kSpace = ' ';
  static constexpr char16_t kNBSP = 0x00A0;
  static constexpr char16_t kGreaterThan = '>';

  /**
   * IsSimplyEditableNode() returns true when aNode is simply editable.
   * This does NOT means that aNode can be removed from current parent nor
   * aNode's data is editable.
   */
  static bool IsSimplyEditableNode(const nsINode& aNode) {
    return aNode.IsEditable();
  }

  /**
   * IsNeverContentEditableElementByUser() returns true if the element's content
   * is never editable by user.  E.g., the content is always replaced by
   * native anonymous node or something.
   */
  static bool IsNeverElementContentsEditableByUser(const nsIContent& aContent) {
    return aContent.IsElement() &&
           (!HTMLEditUtils::IsContainerNode(aContent) ||
            aContent.IsAnyOfHTMLElements(
                nsGkAtoms::applet, nsGkAtoms::colgroup, nsGkAtoms::frameset,
                nsGkAtoms::head, nsGkAtoms::html, nsGkAtoms::iframe,
                nsGkAtoms::meter, nsGkAtoms::picture, nsGkAtoms::progress,
                nsGkAtoms::select, nsGkAtoms::textarea));
  }

  /**
   * IsNonEditableReplacedContent() returns true when aContent is an inclusive
   * descendant of a replaced element whose content shouldn't be editable by
   * user's operation.
   */
  static bool IsNonEditableReplacedContent(const nsIContent& aContent) {
    for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
      if (element->IsAnyOfHTMLElements(nsGkAtoms::select, nsGkAtoms::option,
                                       nsGkAtoms::optgroup)) {
        return true;
      }
    }
    return false;
  }

  /*
   * IsRemovalNode() returns true when parent of aContent is editable even
   * if aContent isn't editable.
   * This is a valid method to check it if you find the content from point
   * of view of siblings or parents of aContent.
   * Note that padding `<br>` element for empty editor and manual native
   * anonymous content should be deletable even after `HTMLEditor` is destroyed
   * because they are owned/managed by `HTMLEditor`.
   */
  static bool IsRemovableNode(const nsIContent& aContent) {
    return EditorUtils::IsPaddingBRElementForEmptyEditor(aContent) ||
           aContent.IsRootOfNativeAnonymousSubtree() ||
           (aContent.GetParentNode() &&
            aContent.GetParentNode()->IsEditable() &&
            &aContent != aContent.OwnerDoc()->GetBody() &&
            &aContent != aContent.OwnerDoc()->GetDocumentElement());
  }

  /**
   * IsRemovableFromParentNode() returns true when aContent is editable, has a
   * parent node and the parent node is also editable.
   * This is a valid method to check it if you find the content from point
   * of view of descendants of aContent.
   * Note that padding `<br>` element for empty editor and manual native
   * anonymous content should be deletable even after `HTMLEditor` is destroyed
   * because they are owned/managed by `HTMLEditor`.
   */
  static bool IsRemovableFromParentNode(const nsIContent& aContent) {
    return EditorUtils::IsPaddingBRElementForEmptyEditor(aContent) ||
           aContent.IsRootOfNativeAnonymousSubtree() ||
           (aContent.IsEditable() && aContent.GetParentNode() &&
            aContent.GetParentNode()->IsEditable() &&
            &aContent != aContent.OwnerDoc()->GetBody() &&
            &aContent != aContent.OwnerDoc()->GetDocumentElement());
  }

  /**
   * CanContentsBeJoined() returns true if aLeftContent and aRightContent can be
   * joined.  At least, Node.nodeName must be same when this returns true.
   */
  enum class StyleDifference {
    // Ignore style information so that callers may join different styled
    // contents.
    Ignore,
    // Compare style information when the contents are any elements.
    CompareIfElements,
    // Compare style information only when the contents are <span> elements.
    CompareIfSpanElements,
  };
  static bool CanContentsBeJoined(const nsIContent& aLeftContent,
                                  const nsIContent& aRightContent,
                                  StyleDifference aStyleDifference);

  /**
   * IsBlockElement() returns true if aContent is an element and it should
   * be treated as a block.  (This does not refer style information.)
   */
  static bool IsBlockElement(const nsIContent& aContent);
  /**
   * IsInlineElement() returns true if aElement is an element node but
   * shouldn't be treated as a block or aElement is not an element.
   * XXX This name is wrong.  Must be renamed to IsInlineContent() or something.
   */
  static bool IsInlineElement(const nsIContent& aContent) {
    return !IsBlockElement(aContent);
  }

  static bool IsInlineStyle(nsINode* aNode);

  /**
   * IsDisplayOutsideInline() returns true if display-outside value is
   * "inside".  This does NOT flush the layout.
   */
  static bool IsDisplayOutsideInline(const Element& aElement);

  /**
   * IsRemovableInlineStyleElement() returns true if aElement is an inline
   * element and can be removed or split to in order to modifying inline
   * styles.
   */
  static bool IsRemovableInlineStyleElement(Element& aElement);
  static bool IsFormatNode(const nsINode* aNode);
  static bool IsNodeThatCanOutdent(nsINode* aNode);
  static bool IsHeader(nsINode& aNode);
  static bool IsListItem(const nsINode* aNode);
  static bool IsTable(nsINode* aNode);
  static bool IsTableRow(nsINode* aNode);
  static bool IsAnyTableElement(const nsINode* aNode);
  static bool IsAnyTableElementButNotTable(nsINode* aNode);
  static bool IsTableCell(const nsINode* aNode);
  static bool IsTableCellOrCaption(nsINode& aNode);
  static bool IsAnyListElement(const nsINode* aNode);
  static bool IsPre(const nsINode* aNode);
  static bool IsImage(nsINode* aNode);
  static bool IsLink(nsINode* aNode);
  static bool IsNamedAnchor(const nsINode* aNode);
  static bool IsMozDiv(nsINode* aNode);
  static bool IsMailCite(const Element& aElement);
  static bool IsFormWidget(const nsINode* aNode);
  static bool SupportsAlignAttr(nsINode& aNode);

  static bool CanNodeContain(const nsINode& aParent, const nsIContent& aChild) {
    switch (aParent.NodeType()) {
      case nsINode::ELEMENT_NODE:
      case nsINode::DOCUMENT_FRAGMENT_NODE:
        return HTMLEditUtils::CanNodeContain(*aParent.NodeInfo()->NameAtom(),
                                             aChild);
    }
    return false;
  }

  static bool CanNodeContain(const nsINode& aParent, nsAtom& aChildNodeName) {
    switch (aParent.NodeType()) {
      case nsINode::ELEMENT_NODE:
      case nsINode::DOCUMENT_FRAGMENT_NODE:
        return HTMLEditUtils::CanNodeContain(*aParent.NodeInfo()->NameAtom(),
                                             aChildNodeName);
    }
    return false;
  }

  static bool CanNodeContain(nsAtom& aParentNodeName,
                             const nsIContent& aChild) {
    switch (aChild.NodeType()) {
      case nsINode::TEXT_NODE:
      case nsINode::ELEMENT_NODE:
      case nsINode::DOCUMENT_FRAGMENT_NODE:
        return HTMLEditUtils::CanNodeContain(aParentNodeName,
                                             *aChild.NodeInfo()->NameAtom());
    }
    return false;
  }

  // XXX Only this overload does not check the node type.  Therefore, only this
  //     treat Document, Comment, CDATASection, etc.
  static bool CanNodeContain(nsAtom& aParentNodeName, nsAtom& aChildNodeName) {
    nsHTMLTag childTagEnum;
    // XXX Should this handle #cdata-section too?
    if (&aChildNodeName == nsGkAtoms::textTagName) {
      childTagEnum = eHTMLTag_text;
    } else {
      childTagEnum = nsHTMLTags::AtomTagToId(&aChildNodeName);
    }

    nsHTMLTag parentTagEnum = nsHTMLTags::AtomTagToId(&aParentNodeName);
    return HTMLEditUtils::CanNodeContain(parentTagEnum, childTagEnum);
  }

  /**
   * CanElementContainParagraph() returns true if aElement can have a <p>
   * element as its child or its descendant.
   */
  static bool CanElementContainParagraph(const Element& aElement) {
    if (HTMLEditUtils::CanNodeContain(aElement, *nsGkAtoms::p)) {
      return true;
    }

    // Even if the element cannot have a <p> element as a child, it can contain
    // <p> element as a descendant if it's one of the following elements.
    if (aElement.IsAnyOfHTMLElements(nsGkAtoms::ol, nsGkAtoms::ul,
                                     nsGkAtoms::dl, nsGkAtoms::table,
                                     nsGkAtoms::thead, nsGkAtoms::tbody,
                                     nsGkAtoms::tfoot, nsGkAtoms::tr)) {
      return true;
    }

    // XXX Otherwise, Chromium checks the CSS box is a block, but we don't do it
    //     for now.
    return false;
  }

  /**
   * IsContainerNode() returns true if aContent is a container node.
   */
  static bool IsContainerNode(const nsIContent& aContent) {
    nsHTMLTag tagEnum;
    // XXX Should this handle #cdata-section too?
    if (aContent.IsText()) {
      tagEnum = eHTMLTag_text;
    } else {
      // XXX Why don't we use nsHTMLTags::AtomTagToId?  Are there some
      //     difference?
      tagEnum = nsHTMLTags::StringTagToId(aContent.NodeName());
    }
    return HTMLEditUtils::IsContainerNode(tagEnum);
  }

  /**
   * IsSplittableNode() returns true if aContent can split.
   */
  static bool IsSplittableNode(const nsIContent& aContent) {
    if (!EditorUtils::IsEditableContent(aContent,
                                        EditorUtils::EditorType::HTML) ||
        !HTMLEditUtils::IsRemovableFromParentNode(aContent)) {
      return false;
    }
    if (aContent.IsElement()) {
      // XXX Perhaps, instead of using container, we should have "splittable"
      //     information in the DB.  E.g., `<template>`, `<script>` elements
      //     can have children, but shouldn't be split.
      return HTMLEditUtils::IsContainerNode(aContent) &&
             !aContent.IsAnyOfHTMLElements(nsGkAtoms::body, nsGkAtoms::head,
                                           nsGkAtoms::html);
    }
    return aContent.IsText() && aContent.Length() > 0;
  }

  /**
   * See execCommand spec:
   * https://w3c.github.io/editing/execCommand.html#non-list-single-line-container
   * https://w3c.github.io/editing/execCommand.html#single-line-container
   */
  static bool IsNonListSingleLineContainer(nsINode& aNode);
  static bool IsSingleLineContainer(nsINode& aNode);

  /**
   * IsVisibleTextNode() returns true if aText has visible text.  If it has
   * only white-spaces and they are collapsed, returns false.
   */
  static bool IsVisibleTextNode(const Text& aText);

  /**
   * IsInVisibleTextFrames() returns true if all text in aText is in visible
   * text frames.  Callers have to guarantee that there is no pending reflow.
   */
  static bool IsInVisibleTextFrames(nsPresContext* aPresContext,
                                    const Text& aText);

  /**
   * IsVisibleBRElement() and IsInvisibleBRElement() return true if aContent is
   * a visible HTML <br> element, i.e., not a padding <br> element for making
   * last line in a block element visible, or an invisible <br> element.
   */
  static bool IsVisibleBRElement(const nsIContent& aContent) {
    if (!aContent.IsHTMLElement(nsGkAtoms::br)) {
      return false;
    }
    // If followed by a block boundary without visible content, it's invisible
    // <br> element.
    return !HTMLEditUtils::GetElementOfImmediateBlockBoundary(
        aContent, WalkTreeDirection::Forward);
  }
  static bool IsInvisibleBRElement(const nsIContent& aContent) {
    return aContent.IsHTMLElement(nsGkAtoms::br) &&
           !HTMLEditUtils::IsVisibleBRElement(aContent);
  }

  /**
   * IsVisiblePreformattedNewLine() and IsInvisiblePreformattedNewLine() return
   * true if the point is preformatted linefeed and it's visible or invisible.
   * If linefeed is immediately before a block boundary, it's invisible.
   *
   * @param aFollowingBlockElement  [out] If the node is followed by a block
   *                                boundary, this is set to the element
   *                                creating the block boundary.
   */
  template <typename EditorDOMPointType>
  static bool IsVisiblePreformattedNewLine(
      const EditorDOMPointType& aPoint,
      Element** aFollowingBlockElement = nullptr) {
    if (aFollowingBlockElement) {
      *aFollowingBlockElement = nullptr;
    }
    if (!aPoint.IsInTextNode() || aPoint.IsEndOfContainer() ||
        !aPoint.IsCharPreformattedNewLine()) {
      return false;
    }
    // If there are some other characters in the text node, it's a visible
    // linefeed.
    if (!aPoint.IsAtLastContent()) {
      if (EditorUtils::IsWhiteSpacePreformatted(*aPoint.ContainerAsText())) {
        return true;
      }
      const nsTextFragment& textFragment =
          aPoint.ContainerAsText()->TextFragment();
      for (uint32_t offset = aPoint.Offset() + 1;
           offset < textFragment.GetLength(); ++offset) {
        char16_t ch = textFragment.CharAt(AssertedCast<int32_t>(offset));
        if (nsCRT::IsAsciiSpace(ch) && ch != HTMLEditUtils::kNewLine) {
          continue;  // ASCII white-space which is collapsed into the linefeed.
        }
        return true;  // There is a visible character after it.
      }
    }
    // If followed by a block boundary without visible content, it's invisible
    // linefeed.
    Element* followingBlockElement =
        HTMLEditUtils::GetElementOfImmediateBlockBoundary(
            *aPoint.ContainerAsText(), WalkTreeDirection::Forward);
    if (aFollowingBlockElement) {
      *aFollowingBlockElement = followingBlockElement;
    }
    return !followingBlockElement;
  }
  template <typename EditorDOMPointType>
  static bool IsInvisiblePreformattedNewLine(
      const EditorDOMPointType& aPoint,
      Element** aFollowingBlockElement = nullptr) {
    if (!aPoint.IsInTextNode() || aPoint.IsEndOfContainer() ||
        !aPoint.IsCharPreformattedNewLine()) {
      if (aFollowingBlockElement) {
        *aFollowingBlockElement = nullptr;
      }
      return false;
    }
    return !IsVisiblePreformattedNewLine(aPoint, aFollowingBlockElement);
  }

  /**
   * ShouldInsertLinefeedCharacter() returns true if the caller should insert
   * a linefeed character instead of <br> element.
   */
  static bool ShouldInsertLinefeedCharacter(EditorDOMPoint& aPointToInsert,
                                            const Element& aEditingHost);

  /**
   * IsEmptyNode() returns false if aNode has some visible content nodes,
   * list elements or table elements.
   *
   * @param aPresContext    Must not be nullptr if
   *                        EmptyCheckOption::SafeToAskLayout is set.
   * @param aNode           The node to check whether it's empty.
   * @param aOptions        You can specify which type of elements are visible
   *                        and/or whether this can access layout information.
   * @param aSeenBR         [Out] Set to true if this meets an <br> element
   *                        before meething visible things.
   */
  enum class EmptyCheckOption {
    TreatSingleBRElementAsVisible,
    TreatListItemAsVisible,
    TreatTableCellAsVisible,
    IgnoreEditableState,  // TODO: Change to "TreatNonEditableContentAsVisible"
    SafeToAskLayout,
  };
  using EmptyCheckOptions = EnumSet<EmptyCheckOption, uint32_t>;
  static bool IsEmptyNode(nsPresContext* aPresContext, const nsINode& aNode,
                          const EmptyCheckOptions& aOptions = {},
                          bool* aSeenBR = nullptr);
  static bool IsEmptyNode(const nsINode& aNode,
                          const EmptyCheckOptions& aOptions = {},
                          bool* aSeenBR = nullptr) {
    MOZ_ASSERT(!aOptions.contains(EmptyCheckOption::SafeToAskLayout));
    return IsEmptyNode(nullptr, aNode, aOptions, aSeenBR);
  }

  /**
   * IsEmptyInlineContent() returns true if aContent is an inline node and it
   * does not have meaningful content.
   */
  static bool IsEmptyInlineContent(const nsIContent& aContent) {
    return HTMLEditUtils::IsInlineElement(aContent) &&
           HTMLEditUtils::IsContainerNode(aContent) &&
           HTMLEditUtils::IsEmptyNode(
               aContent, {EmptyCheckOption::TreatSingleBRElementAsVisible});
  }

  /**
   * IsEmptyBlockElement() returns true if aElement is a block level element
   * and it doesn't have any visible content.
   */
  static bool IsEmptyBlockElement(const Element& aElement,
                                  const EmptyCheckOptions& aOptions) {
    return HTMLEditUtils::IsBlockElement(aElement) &&
           HTMLEditUtils::IsEmptyNode(aElement, aOptions);
  }

  /**
   * IsEmptyOneHardLine() returns true if aArrayOfContents does not represent
   * 2 or more lines and have meaningful content.
   */
  static bool IsEmptyOneHardLine(
      nsTArray<OwningNonNull<nsIContent>>& aArrayOfContents) {
    if (NS_WARN_IF(aArrayOfContents.IsEmpty())) {
      return true;
    }

    bool brElementHasFound = false;
    for (OwningNonNull<nsIContent>& content : aArrayOfContents) {
      if (!EditorUtils::IsEditableContent(content,
                                          EditorUtils::EditorType::HTML)) {
        continue;
      }
      if (content->IsHTMLElement(nsGkAtoms::br)) {
        // If there are 2 or more `<br>` elements, it's not empty line since
        // there may be only one `<br>` element in a hard line.
        if (brElementHasFound) {
          return false;
        }
        brElementHasFound = true;
        continue;
      }
      if (!HTMLEditUtils::IsEmptyInlineContent(content)) {
        return false;
      }
    }
    return true;
  }

  /**
   * IsPointAtEdgeOfLink() returns true if aPoint is at start or end of a
   * link.
   */
  template <typename PT, typename CT>
  static bool IsPointAtEdgeOfLink(const EditorDOMPointBase<PT, CT>& aPoint,
                                  Element** aFoundLinkElement = nullptr) {
    if (aFoundLinkElement) {
      *aFoundLinkElement = nullptr;
    }
    if (!aPoint.IsInContentNode()) {
      return false;
    }
    if (!aPoint.IsStartOfContainer() && !aPoint.IsEndOfContainer()) {
      return false;
    }
    // XXX Assuming it's not in an empty text node because it's unrealistic edge
    //     case.
    bool maybeStartOfAnchor = aPoint.IsStartOfContainer();
    for (EditorRawDOMPoint point(aPoint.ContainerAsContent());
         point.IsSet() && (maybeStartOfAnchor ? point.IsStartOfContainer()
                                              : point.IsAtLastContent());
         point = point.ParentPoint()) {
      if (HTMLEditUtils::IsLink(point.GetContainer())) {
        // Now, we're at start or end of <a href>.
        if (aFoundLinkElement) {
          *aFoundLinkElement = do_AddRef(point.ContainerAsElement()).take();
        }
        return true;
      }
    }
    return false;
  }

  /**
   * IsContentInclusiveDescendantOfLink() returns true if aContent is a
   * descendant of a link element.
   * Note that this returns true even if editing host of aContent is in a link
   * element.
   */
  static bool IsContentInclusiveDescendantOfLink(
      nsIContent& aContent, Element** aFoundLinkElement = nullptr) {
    if (aFoundLinkElement) {
      *aFoundLinkElement = nullptr;
    }
    for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
      if (HTMLEditUtils::IsLink(element)) {
        if (aFoundLinkElement) {
          *aFoundLinkElement = do_AddRef(element).take();
        }
        return true;
      }
    }
    return false;
  }

  /**
   * IsRangeEntirelyInLink() returns true if aRange is entirely in a link
   * element.
   * Note that this returns true even if editing host of the range is in a link
   * element.
   */
  template <typename EditorDOMRangeType>
  static bool IsRangeEntirelyInLink(const EditorDOMRangeType& aRange,
                                    Element** aFoundLinkElement = nullptr) {
    MOZ_ASSERT(aRange.IsPositionedAndValid());
    if (aFoundLinkElement) {
      *aFoundLinkElement = nullptr;
    }
    nsINode* commonAncestorNode =
        nsContentUtils::GetClosestCommonInclusiveAncestor(
            aRange.StartRef().GetContainer(), aRange.EndRef().GetContainer());
    if (NS_WARN_IF(!commonAncestorNode) || !commonAncestorNode->IsContent()) {
      return false;
    }
    return IsContentInclusiveDescendantOfLink(*commonAncestorNode->AsContent(),
                                              aFoundLinkElement);
  }

  /**
   * Get adjacent content node of aNode if there is (even if one is in different
   * parent element).
   *
   * @param aNode               The node from which we start to walk the DOM
   *                            tree.
   * @param aOptions            See WalkTreeOption for the detail.
   * @param aAncestorLimiter    Ancestor limiter element which these methods
   *                            never cross its boundary.  This is typically
   *                            the editing host.
   */
  enum class WalkTreeOption {
    IgnoreNonEditableNode,     // Ignore non-editable nodes and their children.
    IgnoreDataNodeExceptText,  // Ignore data nodes which are not text node.
    IgnoreWhiteSpaceOnlyText,  // Ignore text nodes having only white-spaces.
    StopAtBlockBoundary,       // Stop waking the tree at a block boundary.
  };
  using WalkTreeOptions = EnumSet<WalkTreeOption>;
  static nsIContent* GetPreviousContent(
      const nsINode& aNode, const WalkTreeOptions& aOptions,
      const Element* aAncestorLimiter = nullptr) {
    if (&aNode == aAncestorLimiter ||
        (aAncestorLimiter &&
         !aNode.IsInclusiveDescendantOf(aAncestorLimiter))) {
      return nullptr;
    }
    return HTMLEditUtils::GetAdjacentContent(aNode, WalkTreeDirection::Backward,
                                             aOptions, aAncestorLimiter);
  }
  static nsIContent* GetNextContent(const nsINode& aNode,
                                    const WalkTreeOptions& aOptions,
                                    const Element* aAncestorLimiter = nullptr) {
    if (&aNode == aAncestorLimiter ||
        (aAncestorLimiter &&
         !aNode.IsInclusiveDescendantOf(aAncestorLimiter))) {
      return nullptr;
    }
    return HTMLEditUtils::GetAdjacentContent(aNode, WalkTreeDirection::Forward,
                                             aOptions, aAncestorLimiter);
  }

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  template <typename PT, typename CT>
  static nsIContent* GetPreviousContent(
      const EditorDOMPointBase<PT, CT>& aPoint, const WalkTreeOptions& aOptions,
      const Element* aAncestorLimiter = nullptr);

  /**
   * And another version that takes a point in DOM tree rather than a node.
   *
   * Note that this may return the child at the offset.  E.g., following code
   * causes infinite loop.
   *
   * EditorRawDOMPoint point(aEditableNode);
   * while (nsIContent* content =
   *          GetNextContent(point, {WalkTreeOption::IgnoreNonEditableNode})) {
   *   // Do something...
   *   point.Set(content);
   * }
   *
   * Following code must be you expected:
   *
   * while (nsIContent* content =
   *          GetNextContent(point, {WalkTreeOption::IgnoreNonEditableNode}) {
   *   // Do something...
   *   DebugOnly<bool> advanced = point.Advanced();
   *   MOZ_ASSERT(advanced);
   *   point.Set(point.GetChild());
   * }
   */
  template <typename PT, typename CT>
  static nsIContent* GetNextContent(const EditorDOMPointBase<PT, CT>& aPoint,
                                    const WalkTreeOptions& aOptions,
                                    const Element* aAncestorLimiter = nullptr);

  /**
   * GetPreviousSibling() and GetNextSibling() return the nearest sibling of
   * aContent which does not match with aOption.
   */
  static nsIContent* GetPreviousSibling(const nsIContent& aContent,
                                        const WalkTreeOptions& aOptions) {
    for (nsIContent* sibling = aContent.GetPreviousSibling(); sibling;
         sibling = sibling->GetPreviousSibling()) {
      if (HTMLEditUtils::IsContentIgnored(*sibling, aOptions)) {
        continue;
      }
      if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
          HTMLEditUtils::IsBlockElement(*sibling)) {
        return nullptr;
      }
      return sibling;
    }
    return nullptr;
  }

  static nsIContent* GetNextSibling(const nsIContent& aContent,
                                    const WalkTreeOptions& aOptions) {
    for (nsIContent* sibling = aContent.GetNextSibling(); sibling;
         sibling = sibling->GetNextSibling()) {
      if (HTMLEditUtils::IsContentIgnored(*sibling, aOptions)) {
        continue;
      }
      if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
          HTMLEditUtils::IsBlockElement(*sibling)) {
        return nullptr;
      }
      return sibling;
    }
    return nullptr;
  }

  /**
   * GetLastChild() and GetFirstChild() return the first or last child of aNode
   * which does not match with aOption.
   */
  static nsIContent* GetLastChild(const nsINode& aNode,
                                  const WalkTreeOptions& aOptions) {
    for (nsIContent* child = aNode.GetLastChild(); child;
         child = child->GetPreviousSibling()) {
      if (HTMLEditUtils::IsContentIgnored(*child, aOptions)) {
        continue;
      }
      if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
          HTMLEditUtils::IsBlockElement(*child)) {
        return nullptr;
      }
      return child;
    }
    return nullptr;
  }

  static nsIContent* GetFirstChild(const nsINode& aNode,
                                   const WalkTreeOptions& aOptions) {
    for (nsIContent* child = aNode.GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (HTMLEditUtils::IsContentIgnored(*child, aOptions)) {
        continue;
      }
      if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
          HTMLEditUtils::IsBlockElement(*child)) {
        return nullptr;
      }
      return child;
    }
    return nullptr;
  }

  static bool IsLastChild(const nsIContent& aContent,
                          const WalkTreeOptions& aOptions) {
    nsINode* parentNode = aContent.GetParentNode();
    if (!parentNode) {
      return false;
    }
    return HTMLEditUtils::GetLastChild(*parentNode, aOptions) == &aContent;
  }

  static bool IsFirstChild(const nsIContent& aContent,
                           const WalkTreeOptions& aOptions) {
    nsINode* parentNode = aContent.GetParentNode();
    if (!parentNode) {
      return false;
    }
    return HTMLEditUtils::GetFirstChild(*parentNode, aOptions) == &aContent;
  }

  /**
   * GetAdjacentContentToPutCaret() walks the DOM tree to find an editable node
   * near aPoint where may be a good point to put caret and keep typing or
   * deleting.
   *
   * @param aPoint      The DOM point where to start to search from.
   * @return            If found, returns non-nullptr.  Otherwise, nullptr.
   *                    Note that if found node is in different table structure
   *                    element, this returns nullptr.
   */
  enum class WalkTreeDirection { Forward, Backward };
  template <typename PT, typename CT>
  static nsIContent* GetAdjacentContentToPutCaret(
      const EditorDOMPointBase<PT, CT>& aPoint,
      WalkTreeDirection aWalkTreeDirection, const Element& aEditingHost) {
    MOZ_ASSERT(aPoint.IsSetAndValid());

    nsIContent* editableContent = nullptr;
    if (aWalkTreeDirection == WalkTreeDirection::Backward) {
      editableContent = HTMLEditUtils::GetPreviousContent(
          aPoint, {WalkTreeOption::IgnoreNonEditableNode}, &aEditingHost);
      if (!editableContent) {
        return nullptr;  // Not illegal.
      }
    } else {
      editableContent = HTMLEditUtils::GetNextContent(
          aPoint, {WalkTreeOption::IgnoreNonEditableNode}, &aEditingHost);
      if (NS_WARN_IF(!editableContent)) {
        // Perhaps, illegal because the node pointed by aPoint isn't editable
        // and nobody of previous nodes is editable.
        return nullptr;
      }
    }

    // scan in the right direction until we find an eligible text node,
    // but don't cross any breaks, images, or table elements.
    // XXX This comment sounds odd.  editableContent may have already crossed
    //     breaks and/or images if they are non-editable.
    while (editableContent && !editableContent->IsText() &&
           !editableContent->IsHTMLElement(nsGkAtoms::br) &&
           !HTMLEditUtils::IsImage(editableContent)) {
      if (aWalkTreeDirection == WalkTreeDirection::Backward) {
        editableContent = HTMLEditUtils::GetPreviousContent(
            *editableContent, {WalkTreeOption::IgnoreNonEditableNode},
            &aEditingHost);
        if (NS_WARN_IF(!editableContent)) {
          return nullptr;
        }
      } else {
        editableContent = HTMLEditUtils::GetNextContent(
            *editableContent, {WalkTreeOption::IgnoreNonEditableNode},
            &aEditingHost);
        if (NS_WARN_IF(!editableContent)) {
          return nullptr;
        }
      }
    }

    // don't cross any table elements
    if ((!aPoint.IsInContentNode() &&
         !!HTMLEditUtils::GetInclusiveAncestorAnyTableElement(
             *editableContent)) ||
        (HTMLEditUtils::GetInclusiveAncestorAnyTableElement(*editableContent) !=
         HTMLEditUtils::GetInclusiveAncestorAnyTableElement(
             *aPoint.ContainerAsContent()))) {
      return nullptr;
    }

    // otherwise, ok, we have found a good spot to put the selection
    return editableContent;
  }

  /**
   * GetLastLeafContent() returns rightmost leaf content in aNode.  It depends
   * on aLeafNodeTypes whether this which types of nodes are treated as leaf
   * nodes.
   */
  enum class LeafNodeType {
    // Even if there is a child block, keep scanning a leaf content in it.
    OnlyLeafNode,
    // If there is a child block, return it too.  Note that this does not
    // mean that block siblings are not treated as leaf nodes.
    LeafNodeOrChildBlock,
    // If there is a non-editable element if and only if scanning from editable
    // node, return it too.
    LeafNodeOrNonEditableNode,
    // Ignore non-editable content at walking the tree.
    OnlyEditableLeafNode,
  };
  using LeafNodeTypes = EnumSet<LeafNodeType>;
  static nsIContent* GetLastLeafContent(
      nsINode& aNode, const LeafNodeTypes& aLeafNodeTypes,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT_IF(
        aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
        !aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode));
    // editor shouldn't touch child nodes which are replaced with native
    // anonymous nodes.
    if (aNode.IsElement() &&
        HTMLEditUtils::IsNeverElementContentsEditableByUser(
            *aNode.AsElement())) {
      return nullptr;
    }
    for (nsIContent* content = aNode.GetLastChild(); content;) {
      if (aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode) &&
          !EditorUtils::IsEditableContent(*content,
                                          EditorUtils::EditorType::HTML)) {
        content = HTMLEditUtils::GetPreviousContent(
            *content, {WalkTreeOption::IgnoreNonEditableNode},
            aAncestorLimiter);
        continue;
      }
      if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrChildBlock) &&
          HTMLEditUtils::IsBlockElement(*content)) {
        return content;
      }
      if (!content->HasChildren() ||
          HTMLEditUtils::IsNeverElementContentsEditableByUser(*content)) {
        return content;
      }
      if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode) &&
          aNode.IsEditable() && !content->IsEditable()) {
        return content;
      }
      content = content->GetLastChild();
    }
    return nullptr;
  }

  /**
   * GetFirstLeafContent() returns leftmost leaf content in aNode.  It depends
   * on aLeafNodeTypes whether this scans into a block child or treat block as a
   * leaf.
   */
  static nsIContent* GetFirstLeafContent(
      const nsINode& aNode, const LeafNodeTypes& aLeafNodeTypes,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT_IF(
        aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
        !aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode));
    // editor shouldn't touch child nodes which are replaced with native
    // anonymous nodes.
    if (aNode.IsElement() &&
        HTMLEditUtils::IsNeverElementContentsEditableByUser(
            *aNode.AsElement())) {
      return nullptr;
    }
    for (nsIContent* content = aNode.GetFirstChild(); content;) {
      if (aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode) &&
          !EditorUtils::IsEditableContent(*content,
                                          EditorUtils::EditorType::HTML)) {
        content = HTMLEditUtils::GetNextContent(
            *content, {WalkTreeOption::IgnoreNonEditableNode},
            aAncestorLimiter);
        continue;
      }
      if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrChildBlock) &&
          HTMLEditUtils::IsBlockElement(*content)) {
        return content;
      }
      if (!content->HasChildren() ||
          HTMLEditUtils::IsNeverElementContentsEditableByUser(*content)) {
        return content;
      }
      if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode) &&
          aNode.IsEditable() && !content->IsEditable()) {
        return content;
      }
      content = content->GetFirstChild();
    }
    return nullptr;
  }

  /**
   * GetNextLeafContentOrNextBlockElement() returns next leaf content or
   * next block element of aStartContent inside aAncestorLimiter.
   * Note that the result may be a contet outside aCurrentBlock if
   * aStartContent equals aCurrentBlock.
   *
   * @param aStartContent       The start content to scan next content.
   * @param aCurrentBlock       Must be ancestor of aStartContent.  Dispite
   *                            the name, inline content is allowed if
   *                            aStartContent is in an inline editing host.
   * @param aLeafNodeTypes      See LeafNodeType.
   * @param aAncestorLimiter    Optional, setting this guarantees the
   *                            result is in aAncestorLimiter unless
   *                            aStartContent is not a descendant of this.
   */
  static nsIContent* GetNextLeafContentOrNextBlockElement(
      const nsIContent& aStartContent, const nsIContent& aCurrentBlock,
      const LeafNodeTypes& aLeafNodeTypes,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT_IF(
        aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
        !aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode));

    if (&aStartContent == aAncestorLimiter) {
      return nullptr;
    }

    nsIContent* nextContent = aStartContent.GetNextSibling();
    if (!nextContent) {
      if (!aStartContent.GetParentElement()) {
        NS_WARNING("Reached orphan node while climbing up the DOM tree");
        return nullptr;
      }
      for (Element* parentElement : aStartContent.AncestorsOfType<Element>()) {
        if (parentElement == &aCurrentBlock) {
          return nullptr;
        }
        if (parentElement == aAncestorLimiter) {
          NS_WARNING("Reached editing host while climbing up the DOM tree");
          return nullptr;
        }
        nextContent = parentElement->GetNextSibling();
        if (nextContent) {
          break;
        }
        if (!parentElement->GetParentElement()) {
          NS_WARNING("Reached orphan node while climbing up the DOM tree");
          return nullptr;
        }
      }
      MOZ_ASSERT(nextContent);
    }

    // We have a next content.  If it's a block, return it.
    if (HTMLEditUtils::IsBlockElement(*nextContent)) {
      return nextContent;
    }
    if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode) &&
        aStartContent.IsEditable() && !nextContent->IsEditable()) {
      return nextContent;
    }
    if (HTMLEditUtils::IsContainerNode(*nextContent)) {
      // Else if it's a container, get deep leftmost child
      if (nsIContent* child = HTMLEditUtils::GetFirstLeafContent(
              *nextContent, aLeafNodeTypes)) {
        return child;
      }
    }
    // Else return the next content itself.
    return nextContent;
  }

  /**
   * Similar to the above method, but take a DOM point to specify scan start
   * point.
   */
  template <typename PT, typename CT>
  static nsIContent* GetNextLeafContentOrNextBlockElement(
      const EditorDOMPointBase<PT, CT>& aStartPoint,
      const nsIContent& aCurrentBlock, const LeafNodeTypes& aLeafNodeTypes,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(aStartPoint.IsSet());
    MOZ_ASSERT_IF(
        aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
        !aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode));
    NS_ASSERTION(!aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
                 "Not implemented yet");

    if (!aStartPoint.IsInContentNode()) {
      return nullptr;
    }
    if (aStartPoint.IsInTextNode()) {
      return HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *aStartPoint.ContainerAsText(), aCurrentBlock, aLeafNodeTypes,
          aAncestorLimiter);
    }
    if (!HTMLEditUtils::IsContainerNode(*aStartPoint.ContainerAsContent())) {
      return HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aLeafNodeTypes,
          aAncestorLimiter);
    }

    nsCOMPtr<nsIContent> nextContent = aStartPoint.GetChild();
    if (!nextContent) {
      if (aStartPoint.GetContainer() == &aCurrentBlock) {
        // We are at end of the block.
        return nullptr;
      }

      // We are at end of non-block container
      return HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aLeafNodeTypes,
          aAncestorLimiter);
    }

    // We have a next node.  If it's a block, return it.
    if (HTMLEditUtils::IsBlockElement(*nextContent)) {
      return nextContent;
    }
    if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode) &&
        aStartPoint.GetContainer()->IsEditable() &&
        !nextContent->IsEditable()) {
      return nextContent;
    }
    if (HTMLEditUtils::IsContainerNode(*nextContent)) {
      // else if it's a container, get deep leftmost child
      if (nsIContent* child = HTMLEditUtils::GetFirstLeafContent(
              *nextContent, aLeafNodeTypes)) {
        return child;
      }
    }
    // Else return the node itself
    return nextContent;
  }

  /**
   * GetPreviousLeafContentOrPreviousBlockElement() returns previous leaf
   * content or previous block element of aStartContent inside
   * aAncestorLimiter.
   * Note that the result may be a contet outside aCurrentBlock if
   * aStartContent equals aCurrentBlock.
   *
   * @param aStartContent       The start content to scan previous content.
   * @param aCurrentBlock       Must be ancestor of aStartContent.  Dispite
   *                            the name, inline content is allowed if
   *                            aStartContent is in an inline editing host.
   * @param aLeafNodeTypes      See LeafNodeType.
   * @param aAncestorLimiter    Optional, setting this guarantees the
   *                            result is in aAncestorLimiter unless
   *                            aStartContent is not a descendant of this.
   */
  static nsIContent* GetPreviousLeafContentOrPreviousBlockElement(
      const nsIContent& aStartContent, const nsIContent& aCurrentBlock,
      const LeafNodeTypes& aLeafNodeTypes,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT_IF(
        aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
        !aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode));
    NS_ASSERTION(!aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
                 "Not implemented yet");

    if (&aStartContent == aAncestorLimiter) {
      return nullptr;
    }

    nsIContent* previousContent = aStartContent.GetPreviousSibling();
    if (!previousContent) {
      if (!aStartContent.GetParentElement()) {
        NS_WARNING("Reached orphan node while climbing up the DOM tree");
        return nullptr;
      }
      for (Element* parentElement : aStartContent.AncestorsOfType<Element>()) {
        if (parentElement == &aCurrentBlock) {
          return nullptr;
        }
        if (parentElement == aAncestorLimiter) {
          NS_WARNING("Reached editing host while climbing up the DOM tree");
          return nullptr;
        }
        previousContent = parentElement->GetPreviousSibling();
        if (previousContent) {
          break;
        }
        if (!parentElement->GetParentElement()) {
          NS_WARNING("Reached orphan node while climbing up the DOM tree");
          return nullptr;
        }
      }
      MOZ_ASSERT(previousContent);
    }

    // We have a next content.  If it's a block, return it.
    if (HTMLEditUtils::IsBlockElement(*previousContent)) {
      return previousContent;
    }
    if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode) &&
        aStartContent.IsEditable() && !previousContent->IsEditable()) {
      return previousContent;
    }
    if (HTMLEditUtils::IsContainerNode(*previousContent)) {
      // Else if it's a container, get deep rightmost child
      if (nsIContent* child = HTMLEditUtils::GetLastLeafContent(
              *previousContent, aLeafNodeTypes)) {
        return child;
      }
    }
    // Else return the next content itself.
    return previousContent;
  }

  /**
   * Similar to the above method, but take a DOM point to specify scan start
   * point.
   */
  template <typename PT, typename CT>
  static nsIContent* GetPreviousLeafContentOrPreviousBlockElement(
      const EditorDOMPointBase<PT, CT>& aStartPoint,
      const nsIContent& aCurrentBlock, const LeafNodeTypes& aLeafNodeTypes,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(aStartPoint.IsSet());
    MOZ_ASSERT_IF(
        aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
        !aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode));
    NS_ASSERTION(!aLeafNodeTypes.contains(LeafNodeType::OnlyEditableLeafNode),
                 "Not implemented yet");

    if (!aStartPoint.IsInContentNode()) {
      return nullptr;
    }
    if (aStartPoint.IsInTextNode()) {
      return HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          *aStartPoint.ContainerAsText(), aCurrentBlock, aLeafNodeTypes,
          aAncestorLimiter);
    }
    if (!HTMLEditUtils::IsContainerNode(*aStartPoint.ContainerAsContent())) {
      return HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aLeafNodeTypes,
          aAncestorLimiter);
    }

    if (aStartPoint.IsStartOfContainer()) {
      if (aStartPoint.GetContainer() == &aCurrentBlock) {
        // We are at start of the block.
        return nullptr;
      }

      // We are at start of non-block container
      return HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aLeafNodeTypes,
          aAncestorLimiter);
    }

    nsCOMPtr<nsIContent> previousContent =
        aStartPoint.GetPreviousSiblingOfChild();
    if (NS_WARN_IF(!previousContent)) {
      return nullptr;
    }

    // We have a prior node.  If it's a block, return it.
    if (HTMLEditUtils::IsBlockElement(*previousContent)) {
      return previousContent;
    }
    if (aLeafNodeTypes.contains(LeafNodeType::LeafNodeOrNonEditableNode) &&
        aStartPoint.GetContainer()->IsEditable() &&
        !previousContent->IsEditable()) {
      return previousContent;
    }
    if (HTMLEditUtils::IsContainerNode(*previousContent)) {
      // Else if it's a container, get deep rightmost child
      if (nsIContent* child = HTMLEditUtils::GetLastLeafContent(
              *previousContent, aLeafNodeTypes)) {
        return child;
      }
    }
    // Else return the node itself
    return previousContent;
  }

  /**
   * Get previous/next editable point from start or end of aContent.
   */
  enum class InvisibleWhiteSpaces {
    Ignore,    // Ignore invisible white-spaces, i.e., don't return middle of
               // them.
    Preserve,  // Preserve invisible white-spaces, i.e., result may be start or
               // end of a text node even if it begins or ends with invisible
               // white-spaces.
  };
  enum class TableBoundary {
    Ignore,                  // May cross any table element boundary.
    NoCrossTableElement,     // Won't cross `<table>` element boundary.
    NoCrossAnyTableElement,  // Won't cross any table element boundary.
  };
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetPreviousEditablePoint(
      nsIContent& aContent, const Element* aAncestorLimiter,
      InvisibleWhiteSpaces aInvisibleWhiteSpaces,
      TableBoundary aHowToTreatTableBoundary);
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetNextEditablePoint(
      nsIContent& aContent, const Element* aAncestorLimiter,
      InvisibleWhiteSpaces aInvisibleWhiteSpaces,
      TableBoundary aHowToTreatTableBoundary);

  /**
   * GetAncestorElement() and GetInclusiveAncestorElement() return
   * (inclusive) block ancestor element of aContent whose time matches
   * aAncestorTypes.
   */
  enum class AncestorType {
    ClosestBlockElement,
    MostDistantInlineElementInBlock,
    EditableElement,
    IgnoreHRElement,  // Ignore ancestor <hr> element since invalid structure
  };
  using AncestorTypes = EnumSet<AncestorType>;
  constexpr static AncestorTypes
      ClosestEditableBlockElementOrInlineEditingHost = {
          AncestorType::ClosestBlockElement,
          AncestorType::MostDistantInlineElementInBlock,
          AncestorType::EditableElement};
  constexpr static AncestorTypes ClosestBlockElement = {
      AncestorType::ClosestBlockElement};
  constexpr static AncestorTypes ClosestEditableBlockElement = {
      AncestorType::ClosestBlockElement, AncestorType::EditableElement};
  constexpr static AncestorTypes ClosestEditableBlockElementExceptHRElement = {
      AncestorType::ClosestBlockElement, AncestorType::IgnoreHRElement,
      AncestorType::EditableElement};
  static Element* GetAncestorElement(const nsIContent& aContent,
                                     const AncestorTypes& aAncestorTypes,
                                     const Element* aAncestorLimiter = nullptr);
  static Element* GetInclusiveAncestorElement(
      const nsIContent& aContent, const AncestorTypes& aAncestorTypes,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetClosestAncestorTableElement() returns the nearest inclusive ancestor
   * <table> element of aContent.
   */
  static Element* GetClosestAncestorTableElement(const nsIContent& aContent) {
    // TODO: the method name and its documentation clash with the
    // implementation. Split this method into
    // `GetClosestAncestorTableElement` and
    // `GetClosestInclusiveAncestorTableElement`.
    if (!aContent.GetParent()) {
      return nullptr;
    }
    for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
      if (HTMLEditUtils::IsTable(element)) {
        return element;
      }
    }
    return nullptr;
  }

  static Element* GetInclusiveAncestorAnyTableElement(
      const nsIContent& aContent) {
    for (Element* parent : aContent.InclusiveAncestorsOfType<Element>()) {
      if (HTMLEditUtils::IsAnyTableElement(parent)) {
        return parent;
      }
    }
    return nullptr;
  }

  static Element* GetClosestAncestorAnyListElement(const nsIContent& aContent);

  /**
   * GetClosestAncestorListItemElement() returns a list item element if
   * aContent or its ancestor in editing host is one.  However, this won't
   * cross table related element.
   */
  static Element* GetClosestAncestorListItemElement(
      const nsIContent& aContent, const Element* aAncestorLimit = nullptr) {
    MOZ_ASSERT_IF(aAncestorLimit,
                  aContent.IsInclusiveDescendantOf(aAncestorLimit));

    if (HTMLEditUtils::IsListItem(&aContent)) {
      return const_cast<Element*>(aContent.AsElement());
    }

    for (Element* parentElement : aContent.AncestorsOfType<Element>()) {
      if (HTMLEditUtils::IsAnyTableElement(parentElement)) {
        return nullptr;
      }
      if (HTMLEditUtils::IsListItem(parentElement)) {
        return parentElement;
      }
      if (parentElement == aAncestorLimit) {
        return nullptr;
      }
    }
    return nullptr;
  }

  /**
   * GetRangeSelectingAllContentInAllListItems() returns a range which selects
   * from start of the first list item to end of the last list item of
   * aListElement.  Note that the result may be in different list element if
   * aListElement has child list element(s) directly.
   */
  template <typename EditorDOMRangeType>
  static EditorDOMRangeType GetRangeSelectingAllContentInAllListItems(
      const Element& aListElement) {
    MOZ_ASSERT(HTMLEditUtils::IsAnyListElement(&aListElement));
    Element* firstListItem =
        HTMLEditUtils::GetFirstListItemElement(aListElement);
    Element* lastListItem = HTMLEditUtils::GetLastListItemElement(aListElement);
    MOZ_ASSERT_IF(firstListItem, lastListItem);
    MOZ_ASSERT_IF(!firstListItem, !lastListItem);
    if (!firstListItem || !lastListItem) {
      return EditorDOMRangeType();
    }
    return EditorDOMRangeType(
        typename EditorDOMRangeType::PointType(
            firstListItem->GetFirstChild() &&
                    firstListItem->GetFirstChild()->IsText()
                ? firstListItem->GetFirstChild()
                : static_cast<nsIContent*>(firstListItem),
            0u),
        EditorDOMRangeType::PointType::AtEndOf(
            lastListItem->GetLastChild() &&
                    lastListItem->GetLastChild()->IsText()
                ? *lastListItem->GetFirstChild()
                : static_cast<nsIContent&>(*lastListItem)));
  }

  /**
   * GetFirstListItemElement() returns the first list item element in the
   * pre-order tree traversal of the DOM.
   */
  static Element* GetFirstListItemElement(const Element& aListElement) {
    MOZ_ASSERT(HTMLEditUtils::IsAnyListElement(&aListElement));
    for (nsIContent* maybeFirstListItem = aListElement.GetFirstChild();
         maybeFirstListItem;
         maybeFirstListItem = maybeFirstListItem->GetNextNode(&aListElement)) {
      if (HTMLEditUtils::IsListItem(maybeFirstListItem)) {
        return maybeFirstListItem->AsElement();
      }
    }
    return nullptr;
  }

  /**
   * GetLastListItemElement() returns the last list item element in the
   * post-order tree traversal of the DOM.  I.e., returns the last list
   * element whose close tag appears at last.
   */
  static Element* GetLastListItemElement(const Element& aListElement) {
    MOZ_ASSERT(HTMLEditUtils::IsAnyListElement(&aListElement));
    for (nsIContent* maybeLastListItem = aListElement.GetLastChild();
         maybeLastListItem;) {
      if (HTMLEditUtils::IsListItem(maybeLastListItem)) {
        return maybeLastListItem->AsElement();
      }
      if (maybeLastListItem->HasChildren()) {
        maybeLastListItem = maybeLastListItem->GetLastChild();
        continue;
      }
      if (maybeLastListItem->GetPreviousSibling()) {
        maybeLastListItem = maybeLastListItem->GetPreviousSibling();
        continue;
      }
      for (Element* parent = maybeLastListItem->GetParentElement(); parent;
           parent = parent->GetParentElement()) {
        maybeLastListItem = nullptr;
        if (parent == &aListElement) {
          return nullptr;
        }
        if (parent->GetPreviousSibling()) {
          maybeLastListItem = parent->GetPreviousSibling();
          break;
        }
      }
    }
    return nullptr;
  }

  /**
   * GetFirstTableCellElementChild() and GetLastTableCellElementChild()
   * return the first/last element child of <tr> element if it's a table
   * cell element.
   */
  static Element* GetFirstTableCellElementChild(
      const Element& aTableRowElement) {
    MOZ_ASSERT(aTableRowElement.IsHTMLElement(nsGkAtoms::tr));
    Element* firstElementChild = aTableRowElement.GetFirstElementChild();
    return firstElementChild && HTMLEditUtils::IsTableCell(firstElementChild)
               ? firstElementChild
               : nullptr;
  }
  static Element* GetLastTableCellElementChild(
      const Element& aTableRowElement) {
    MOZ_ASSERT(aTableRowElement.IsHTMLElement(nsGkAtoms::tr));
    Element* lastElementChild = aTableRowElement.GetLastElementChild();
    return lastElementChild && HTMLEditUtils::IsTableCell(lastElementChild)
               ? lastElementChild
               : nullptr;
  }

  /**
   * GetPreviousTableCellElementSibling() and GetNextTableCellElementSibling()
   * return a table cell element of previous/next element sibling of given
   * content node if and only if the element sibling is a table cell element.
   */
  static Element* GetPreviousTableCellElementSibling(
      const nsIContent& aChildOfTableRow) {
    MOZ_ASSERT(aChildOfTableRow.GetParentNode());
    MOZ_ASSERT(aChildOfTableRow.GetParentNode()->IsHTMLElement(nsGkAtoms::tr));
    Element* previousElementSibling =
        aChildOfTableRow.GetPreviousElementSibling();
    return previousElementSibling &&
                   HTMLEditUtils::IsTableCell(previousElementSibling)
               ? previousElementSibling
               : nullptr;
  }
  static Element* GetNextTableCellElementSibling(
      const nsIContent& aChildOfTableRow) {
    MOZ_ASSERT(aChildOfTableRow.GetParentNode());
    MOZ_ASSERT(aChildOfTableRow.GetParentNode()->IsHTMLElement(nsGkAtoms::tr));
    Element* nextElementSibling = aChildOfTableRow.GetNextElementSibling();
    return nextElementSibling && HTMLEditUtils::IsTableCell(nextElementSibling)
               ? nextElementSibling
               : nullptr;
  }

  /**
   * GetMostDistantAncestorInlineElement() returns the most distant ancestor
   * inline element between aContent and the aEditingHost.  Even if aEditingHost
   * is an inline element, this method never returns aEditingHost as the result.
   */
  static nsIContent* GetMostDistantAncestorInlineElement(
      const nsIContent& aContent, const Element* aEditingHost = nullptr) {
    if (HTMLEditUtils::IsBlockElement(aContent)) {
      return nullptr;
    }

    // If aNode is the editing host itself, there is no modifiable inline
    // parent.
    if (&aContent == aEditingHost) {
      return nullptr;
    }

    // If aNode is outside of the <body> element, we don't support to edit
    // such elements for now.
    // XXX This should be MOZ_ASSERT after fixing bug 1413131 for avoiding
    //     calling this expensive method.
    if (aEditingHost && !aContent.IsInclusiveDescendantOf(aEditingHost)) {
      return nullptr;
    }

    if (!aContent.GetParent()) {
      return const_cast<nsIContent*>(&aContent);
    }

    // Looks for the highest inline parent in the editing host.
    nsIContent* topMostInlineContent = const_cast<nsIContent*>(&aContent);
    for (nsIContent* content : aContent.AncestorsOfType<nsIContent>()) {
      if (content == aEditingHost ||
          !HTMLEditUtils::IsInlineElement(*content)) {
        break;
      }
      topMostInlineContent = content;
    }
    return topMostInlineContent;
  }

  /**
   * GetMostDistantAnscestorEditableEmptyInlineElement() returns most distant
   * ancestor which only has aEmptyContent or its ancestor, editable and
   * inline element.
   */
  static Element* GetMostDistantAnscestorEditableEmptyInlineElement(
      const nsIContent& aEmptyContent, const Element* aEditingHost = nullptr) {
    nsIContent* lastEmptyContent = const_cast<nsIContent*>(&aEmptyContent);
    for (Element* element = aEmptyContent.GetParentElement();
         element && element != aEditingHost &&
         HTMLEditUtils::IsInlineElement(*element) &&
         HTMLEditUtils::IsSimplyEditableNode(*element);
         element = element->GetParentElement()) {
      if (element->GetChildCount() > 1) {
        for (const nsIContent* child = element->GetFirstChild(); child;
             child = child->GetNextSibling()) {
          if (child == lastEmptyContent || child->IsComment()) {
            continue;
          }
          return lastEmptyContent != &aEmptyContent
                     ? lastEmptyContent->AsElement()
                     : nullptr;
        }
      }
      lastEmptyContent = element;
    }
    return lastEmptyContent != &aEmptyContent ? lastEmptyContent->AsElement()
                                              : nullptr;
  }

  /**
   * GetElementIfOnlyOneSelected() returns an element if aRange selects only
   * the element node (and its descendants).
   */
  static Element* GetElementIfOnlyOneSelected(const AbstractRange& aRange) {
    return GetElementIfOnlyOneSelected(EditorRawDOMRange(aRange));
  }
  template <typename EditorDOMPointType>
  static Element* GetElementIfOnlyOneSelected(
      const EditorDOMRangeBase<EditorDOMPointType>& aRange) {
    if (!aRange.IsPositioned() || aRange.Collapsed()) {
      return nullptr;
    }
    const auto& start = aRange.StartRef();
    const auto& end = aRange.EndRef();
    if (NS_WARN_IF(!start.IsSetAndValid()) ||
        NS_WARN_IF(!end.IsSetAndValid()) ||
        start.GetContainer() != end.GetContainer()) {
      return nullptr;
    }
    nsIContent* childAtStart = start.GetChild();
    if (!childAtStart || !childAtStart->IsElement()) {
      return nullptr;
    }
    // If start child is not the last sibling and only if end child is its
    // next sibling, the start child is selected.
    if (childAtStart->GetNextSibling()) {
      return childAtStart->GetNextSibling() == end.GetChild()
                 ? childAtStart->AsElement()
                 : nullptr;
    }
    // If start child is the last sibling and only if no child at the end,
    // the start child is selected.
    return !end.GetChild() ? childAtStart->AsElement() : nullptr;
  }

  static Element* GetTableCellElementIfOnlyOneSelected(
      const AbstractRange& aRange) {
    Element* element = HTMLEditUtils::GetElementIfOnlyOneSelected(aRange);
    return element && HTMLEditUtils::IsTableCell(element) ? element : nullptr;
  }

  /**
   * GetFirstSelectedTableCellElement() returns a table cell element (i.e.,
   * `<td>` or `<th>` if and only if first selection range selects only a
   * table cell element.
   */
  static Element* GetFirstSelectedTableCellElement(
      const Selection& aSelection) {
    if (!aSelection.RangeCount()) {
      return nullptr;
    }
    const nsRange* firstRange = aSelection.GetRangeAt(0);
    if (NS_WARN_IF(!firstRange) || NS_WARN_IF(!firstRange->IsPositioned())) {
      return nullptr;
    }
    return GetTableCellElementIfOnlyOneSelected(*firstRange);
  }

  /**
   * GetInclusiveFirstChildWhichHasOneChild() returns the deepest element whose
   * tag name is one of `aFirstElementName` and `aOtherElementNames...` if and
   * only if the elements have only one child node.   In other words, when
   * this method meets an element which does not matches any of the tag name
   * or it has no children or 2+ children.
   *
   * XXX This method must be implemented without treating edge cases.  So, the
   *     behavior is odd.  E.g., why can we ignore non-editable node at counting
   *     each children?  Why do we dig non-editable aNode or first child of its
   *     descendants?
   */
  template <typename FirstElementName, typename... OtherElementNames>
  static Element* GetInclusiveDeepestFirstChildWhichHasOneChild(
      const nsINode& aNode, const WalkTreeOptions& aOptions,
      FirstElementName aFirstElementName,
      OtherElementNames... aOtherElementNames) {
    if (!aNode.IsElement()) {
      return nullptr;
    }
    Element* parentElement = nullptr;
    for (nsIContent* content = const_cast<nsIContent*>(aNode.AsContent());
         content && content->IsElement() &&
         content->IsAnyOfHTMLElements(aFirstElementName, aOtherElementNames...);
         // XXX Why do we scan only the first child of every element?  If it's
         //     not editable, why do we ignore it when aOptions specifies so.
         content = content->GetFirstChild()) {
      if (HTMLEditUtils::CountChildren(*content, aOptions) != 1) {
        return content->AsElement();
      }
      parentElement = content->AsElement();
    }
    return parentElement;
  }

  /**
   * IsInTableCellSelectionMode() returns true when Gecko's editor thinks that
   * selection is in a table cell selection mode.
   * Note that Gecko's editor traditionally treats selection as in table cell
   * selection mode when first range selects a table cell element.  I.e., even
   * if `nsFrameSelection` is not in table cell selection mode, this may return
   * true.
   */
  static bool IsInTableCellSelectionMode(const Selection& aSelection) {
    return GetFirstSelectedTableCellElement(aSelection) != nullptr;
  }

  static EditAction GetEditActionForInsert(const nsAtom& aTagName);
  static EditAction GetEditActionForRemoveList(const nsAtom& aTagName);
  static EditAction GetEditActionForInsert(const Element& aElement);
  static EditAction GetEditActionForFormatText(const nsAtom& aProperty,
                                               const nsAtom* aAttribute,
                                               bool aToSetStyle);
  static EditAction GetEditActionForAlignment(const nsAString& aAlignType);

  /**
   * GetPreviousNonCollapsibleCharOffset() returns offset of previous
   * character which is not collapsible white-space characters.
   */
  enum class WalkTextOption {
    TreatNBSPsCollapsible,
  };
  using WalkTextOptions = EnumSet<WalkTextOption>;
  static Maybe<uint32_t> GetPreviousNonCollapsibleCharOffset(
      const EditorDOMPointInText& aPoint,
      const WalkTextOptions& aWalkTextOptions = {}) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetPreviousNonCollapsibleCharOffset(
        *aPoint.ContainerAsText(), aPoint.Offset(), aWalkTextOptions);
  }
  static Maybe<uint32_t> GetPreviousNonCollapsibleCharOffset(
      const Text& aTextNode, uint32_t aOffset,
      const WalkTextOptions& aWalkTextOptions = {}) {
    const bool isWhiteSpaceCollapsible =
        !EditorUtils::IsWhiteSpacePreformatted(aTextNode);
    const bool isNewLineCollapsible =
        !EditorUtils::IsNewLinePreformatted(aTextNode);
    const bool isNBSPCollapsible =
        isWhiteSpaceCollapsible &&
        aWalkTextOptions.contains(WalkTextOption::TreatNBSPsCollapsible);
    const nsTextFragment& textFragment = aTextNode.TextFragment();
    MOZ_ASSERT(aOffset <= textFragment.GetLength());
    for (uint32_t i = aOffset; i; i--) {
      // TODO: Perhaps, nsTextFragment should have scanner methods because
      //       the text may be in per-one-byte storage or per-two-byte storage,
      //       and `CharAt` needs to check it everytime.
      switch (textFragment.CharAt(i - 1)) {
        case HTMLEditUtils::kSpace:
        case HTMLEditUtils::kCarriageReturn:
        case HTMLEditUtils::kTab:
          if (!isWhiteSpaceCollapsible) {
            return Some(i - 1);
          }
          break;
        case HTMLEditUtils::kNewLine:
          if (!isNewLineCollapsible) {
            return Some(i - 1);
          }
          break;
        case HTMLEditUtils::kNBSP:
          if (!isNBSPCollapsible) {
            return Some(i - 1);
          }
          break;
        default:
          MOZ_ASSERT(!nsCRT::IsAsciiSpace(textFragment.CharAt(i - 1)));
          return Some(i - 1);
      }
    }
    return Nothing();
  }

  /**
   * GetNextNonCollapsibleCharOffset() returns offset of next character which is
   * not collapsible white-space characters.
   */
  static Maybe<uint32_t> GetNextNonCollapsibleCharOffset(
      const EditorDOMPointInText& aPoint,
      const WalkTextOptions& aWalkTextOptions = {}) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetNextNonCollapsibleCharOffset(*aPoint.ContainerAsText(),
                                           aPoint.Offset(), aWalkTextOptions);
  }
  static Maybe<uint32_t> GetNextNonCollapsibleCharOffset(
      const Text& aTextNode, uint32_t aOffset,
      const WalkTextOptions& aWalkTextOptions = {}) {
    return GetInclusiveNextNonCollapsibleCharOffset(aTextNode, aOffset + 1,
                                                    aWalkTextOptions);
  }

  /**
   * GetInclusiveNextNonCollapsibleCharOffset() returns offset of inclusive next
   * character which is not collapsible white-space characters.
   */
  static Maybe<uint32_t> GetInclusiveNextNonCollapsibleCharOffset(
      const EditorDOMPointInText& aPoint,
      const WalkTextOptions& aWalkTextOptions = {}) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetInclusiveNextNonCollapsibleCharOffset(
        *aPoint.ContainerAsText(), aPoint.Offset(), aWalkTextOptions);
  }
  static Maybe<uint32_t> GetInclusiveNextNonCollapsibleCharOffset(
      const Text& aTextNode, uint32_t aOffset,
      const WalkTextOptions& aWalkTextOptions = {}) {
    const bool isWhiteSpaceCollapsible =
        !EditorUtils::IsWhiteSpacePreformatted(aTextNode);
    const bool isNewLineCollapsible =
        !EditorUtils::IsNewLinePreformatted(aTextNode);
    const bool isNBSPCollapsible =
        isWhiteSpaceCollapsible &&
        aWalkTextOptions.contains(WalkTextOption::TreatNBSPsCollapsible);
    const nsTextFragment& textFragment = aTextNode.TextFragment();
    MOZ_ASSERT(aOffset <= textFragment.GetLength());
    for (uint32_t i = aOffset; i < textFragment.GetLength(); i++) {
      // TODO: Perhaps, nsTextFragment should have scanner methods because
      //       the text may be in per-one-byte storage or per-two-byte storage,
      //       and `CharAt` needs to check it everytime.
      switch (textFragment.CharAt(i)) {
        case HTMLEditUtils::kSpace:
        case HTMLEditUtils::kCarriageReturn:
        case HTMLEditUtils::kTab:
          if (!isWhiteSpaceCollapsible) {
            return Some(i);
          }
          break;
        case HTMLEditUtils::kNewLine:
          if (!isNewLineCollapsible) {
            return Some(i);
          }
          break;
        case HTMLEditUtils::kNBSP:
          if (!isNBSPCollapsible) {
            return Some(i);
          }
          break;
        default:
          MOZ_ASSERT(!nsCRT::IsAsciiSpace(textFragment.CharAt(i)));
          return Some(i);
      }
    }
    return Nothing();
  }

  /**
   * GetFirstWhiteSpaceOffsetCollapsedWith() returns first collapsible
   * white-space offset which is collapsed with a white-space at the given
   * position.  I.e., the character at the position must be a collapsible
   * white-space.
   */
  static uint32_t GetFirstWhiteSpaceOffsetCollapsedWith(
      const EditorDOMPointInText& aPoint,
      const WalkTextOptions& aWalkTextOptions = {}) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    MOZ_ASSERT(!aPoint.IsEndOfContainer());
    MOZ_ASSERT_IF(
        aWalkTextOptions.contains(WalkTextOption::TreatNBSPsCollapsible),
        aPoint.IsCharCollapsibleASCIISpaceOrNBSP());
    MOZ_ASSERT_IF(
        !aWalkTextOptions.contains(WalkTextOption::TreatNBSPsCollapsible),
        aPoint.IsCharCollapsibleASCIISpace());
    return GetFirstWhiteSpaceOffsetCollapsedWith(
        *aPoint.ContainerAsText(), aPoint.Offset(), aWalkTextOptions);
  }
  static uint32_t GetFirstWhiteSpaceOffsetCollapsedWith(
      const Text& aTextNode, uint32_t aOffset,
      const WalkTextOptions& aWalkTextOptions = {}) {
    MOZ_ASSERT(aOffset < aTextNode.TextLength());
    MOZ_ASSERT_IF(
        aWalkTextOptions.contains(WalkTextOption::TreatNBSPsCollapsible),
        EditorRawDOMPoint(&aTextNode, aOffset)
            .IsCharCollapsibleASCIISpaceOrNBSP());
    MOZ_ASSERT_IF(
        !aWalkTextOptions.contains(WalkTextOption::TreatNBSPsCollapsible),
        EditorRawDOMPoint(&aTextNode, aOffset).IsCharCollapsibleASCIISpace());
    if (!aOffset) {
      return 0;
    }
    Maybe<uint32_t> previousVisibleCharOffset =
        GetPreviousNonCollapsibleCharOffset(aTextNode, aOffset,
                                            aWalkTextOptions);
    return previousVisibleCharOffset.isSome()
               ? previousVisibleCharOffset.value() + 1
               : 0;
  }

  /**
   * GetPreviousPreformattedNewLineInTextNode() returns a point which points
   * previous preformatted linefeed if there is and aPoint is in a text node.
   * If the node's linefeed characters are not preformatted or aPoint is not
   * in a text node, this returns unset DOM point.
   */
  template <typename EditorDOMPointType, typename ArgEditorDOMPointType>
  static EditorDOMPointType GetPreviousPreformattedNewLineInTextNode(
      const ArgEditorDOMPointType& aPoint) {
    if (!aPoint.IsInTextNode() || aPoint.IsStartOfContainer() ||
        !EditorUtils::IsNewLinePreformatted(*aPoint.ContainerAsText())) {
      return EditorDOMPointType();
    }
    Text* textNode = aPoint.ContainerAsText();
    const nsTextFragment& textFragment = textNode->TextFragment();
    MOZ_ASSERT(aPoint.Offset() <= textFragment.GetLength());
    for (uint32_t offset = aPoint.Offset(); offset; --offset) {
      if (textFragment.CharAt(offset - 1) == HTMLEditUtils::kNewLine) {
        return EditorDOMPointType(textNode, offset - 1);
      }
    }
    return EditorDOMPointType();
  }

  /**
   * GetInclusiveNextPreformattedNewLineInTextNode() returns a point which
   * points inclusive next preformatted linefeed if there is and aPoint is in a
   * text node. If the node's linefeed characters are not preformatted or aPoint
   * is not in a text node, this returns unset DOM point.
   */
  template <typename EditorDOMPointType, typename ArgEditorDOMPointType>
  static EditorDOMPointType GetInclusiveNextPreformattedNewLineInTextNode(
      const ArgEditorDOMPointType& aPoint) {
    if (!aPoint.IsInTextNode() || aPoint.IsEndOfContainer() ||
        !EditorUtils::IsNewLinePreformatted(*aPoint.ContainerAsText())) {
      return EditorDOMPointType();
    }
    Text* textNode = aPoint.ContainerAsText();
    const nsTextFragment& textFragment = textNode->TextFragment();
    for (uint32_t offset = aPoint.Offset(); offset < textFragment.GetLength();
         ++offset) {
      if (textFragment.CharAt(offset) == HTMLEditUtils::kNewLine) {
        return EditorDOMPointType(textNode, offset);
      }
    }
    return EditorDOMPointType();
  }

  /**
   * GetGoodCaretPointFor() returns a good point to collapse `Selection`
   * after handling edit action with aDirectionAndAmount.
   *
   * @param aContent            The content where you want to put caret
   *                            around.
   * @param aDirectionAndAmount Muse be one of eNext, eNextWord, eToEndOfLine,
   *                            ePrevious, ePreviousWord and eToBeggingOfLine.
   *                            Set the direction of handled edit action.
   */
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetGoodCaretPointFor(
      nsIContent& aContent, nsIEditor::EDirection aDirectionAndAmount) {
    MOZ_ASSERT(aDirectionAndAmount == nsIEditor::eNext ||
               aDirectionAndAmount == nsIEditor::eNextWord ||
               aDirectionAndAmount == nsIEditor::ePrevious ||
               aDirectionAndAmount == nsIEditor::ePreviousWord ||
               aDirectionAndAmount == nsIEditor::eToBeginningOfLine ||
               aDirectionAndAmount == nsIEditor::eToEndOfLine);

    const bool goingForward = (aDirectionAndAmount == nsIEditor::eNext ||
                               aDirectionAndAmount == nsIEditor::eNextWord ||
                               aDirectionAndAmount == nsIEditor::eToEndOfLine);

    // XXX Why don't we check whether the candidate position is enable or not?
    //     When the result is not editable point, caret will be enclosed in
    //     the non-editable content.

    // If we can put caret in aContent, return start or end in it.
    if (aContent.IsText() || HTMLEditUtils::IsContainerNode(aContent) ||
        NS_WARN_IF(!aContent.GetParentNode())) {
      return EditorDOMPointType(&aContent,
                                goingForward ? 0 : aContent.Length());
    }

    // If we are going forward, put caret at aContent itself.
    if (goingForward) {
      return EditorDOMPointType(&aContent);
    }

    // If we are going backward, put caret to next node unless aContent is an
    // invisible `<br>` element.
    // XXX Shouldn't we put caret to first leaf of the next node?
    if (!HTMLEditUtils::IsInvisibleBRElement(aContent)) {
      EditorDOMPointType ret(EditorDOMPointType::After(aContent));
      NS_WARNING_ASSERTION(ret.IsSet(), "Failed to set after aContent");
      return ret;
    }

    // Otherwise, we should put caret at the invisible `<br>` element.
    return EditorDOMPointType(&aContent);
  }

  /**
   * GetBetterInsertionPointFor() returns better insertion point to insert
   * aContentToInsert.
   *
   * @param aContentToInsert    The content to insert.
   * @param aPointToInsert      A candidate point to insert the node.
   * @param aEditingHost        The editing host containing aPointToInsert.
   * @return                    Better insertion point if next visible node
   *                            is a <br> element and previous visible node
   *                            is neither none, another <br> element nor
   *                            different block level element.
   */
  template <typename EditorDOMPointType, typename EditorDOMPointTypeInput>
  static EditorDOMPointType GetBetterInsertionPointFor(
      const nsIContent& aContentToInsert,
      const EditorDOMPointTypeInput& aPointToInsert,
      const Element& aEditingHost);

  /**
   * Content-based query returns true if <aProperty aAttribute=aValue> effects
   * aNode.  If <aProperty aAttribute=aValue> contains aNode, but
   * <aProperty aAttribute=SomeOtherValue> also contains aNode and the second is
   * more deeply nested than the first, then the first does not effect aNode.
   *
   * @param aNode       The target of the query
   * @param aProperty   The property that we are querying for
   * @param aAttribute  The attribute of aProperty, example: color in
   *                    <FONT color="blue"> May be null.
   * @param aValue      The value of aAttribute, example: blue in
   *                    <FONT color="blue"> May be null.  Ignored if aAttribute
   *                    is null.
   * @param aOutValue   [OUT] the value of the attribute, if aIsSet is true
   * @return            true if <aProperty aAttribute=aValue> effects
   *                    aNode.
   *
   * The nsIContent variant returns aIsSet instead of using an out parameter.
   */
  static bool IsInlineStyleSetByElement(const nsIContent& aContent,
                                        const nsAtom& aProperty,
                                        const nsAtom* aAttribute,
                                        const nsAString* aValue,
                                        nsAString* aOutValue = nullptr) {
    for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
      if (&aProperty != element->NodeInfo()->NameAtom()) {
        continue;
      }
      if (!aAttribute) {
        return true;
      }
      nsAutoString value;
      element->GetAttr(kNameSpaceID_None, aAttribute, value);
      if (aOutValue) {
        *aOutValue = value;
      }
      if (!value.IsEmpty()) {
        if (!aValue) {
          return true;
        }
        if (aValue->Equals(value, nsCaseInsensitiveStringComparator)) {
          return true;
        }
        // We found the prop with the attribute, but the value doesn't match.
        return false;
      }
    }
    return false;
  }

  /**
   * CollectChildren() collects child nodes of aNode (starting from
   * first editable child, but may return non-editable children after it).
   *
   * @param aNode               Parent node of retrieving children.
   * @param aOutArrayOfContents [out] This method will inserts found children
   *                            into this array.
   * @param aIndexToInsertChildren      Starting from this index, found
   *                                    children will be inserted to the array.
   * @param aOptions            Options to scan the children.
   * @return                    Number of found children.
   */
  static size_t CollectChildren(
      nsINode& aNode, nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
      size_t aIndexToInsertChildren, const CollectChildrenOptions& aOptions);

 private:
  static bool CanNodeContain(nsHTMLTag aParentTagId, nsHTMLTag aChildTagId);
  static bool IsContainerNode(nsHTMLTag aTagId);

  static bool CanCrossContentBoundary(nsIContent& aContent,
                                      TableBoundary aHowToTreatTableBoundary) {
    const bool cannotCrossBoundary =
        (aHowToTreatTableBoundary == TableBoundary::NoCrossAnyTableElement &&
         HTMLEditUtils::IsAnyTableElement(&aContent)) ||
        (aHowToTreatTableBoundary == TableBoundary::NoCrossTableElement &&
         aContent.IsHTMLElement(nsGkAtoms::table));
    return !cannotCrossBoundary;
  }

  static bool IsContentIgnored(const nsIContent& aContent,
                               const WalkTreeOptions& aOptions) {
    if (aOptions.contains(WalkTreeOption::IgnoreNonEditableNode) &&
        !EditorUtils::IsEditableContent(aContent,
                                        EditorUtils::EditorType::HTML)) {
      return true;
    }
    if (aOptions.contains(WalkTreeOption::IgnoreDataNodeExceptText) &&
        !EditorUtils::IsElementOrText(aContent)) {
      return true;
    }
    if (aOptions.contains(WalkTreeOption::IgnoreWhiteSpaceOnlyText) &&
        aContent.IsText() &&
        const_cast<Text*>(aContent.AsText())->TextIsOnlyWhitespace()) {
      return true;
    }
    return false;
  }

  static uint32_t CountChildren(const nsINode& aNode,
                                const WalkTreeOptions& aOptions) {
    uint32_t count = 0;
    for (nsIContent* child = aNode.GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (HTMLEditUtils::IsContentIgnored(*child, aOptions)) {
        continue;
      }
      if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
          HTMLEditUtils::IsBlockElement(*child)) {
        break;
      }
      ++count;
    }
    return count;
  }

  /**
   * Helper for GetPreviousContent() and GetNextContent().
   */
  static nsIContent* GetAdjacentLeafContent(
      const nsINode& aNode, WalkTreeDirection aWalkTreeDirection,
      const WalkTreeOptions& aOptions,
      const Element* aAncestorLimiter = nullptr);
  static nsIContent* GetAdjacentContent(
      const nsINode& aNode, WalkTreeDirection aWalkTreeDirection,
      const WalkTreeOptions& aOptions,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetElementOfImmediateBlockBoundary() returns a block element if its
   * block boundary and aContent may be first visible thing before/after the
   * boundary.  And it may return a <br> element only when aContent is a
   * text node and follows a <br> element because only in this case, the
   * start white-spaces are invisible.  So the <br> element works same as
   * a block boundary.
   */
  static Element* GetElementOfImmediateBlockBoundary(
      const nsIContent& aContent, const WalkTreeDirection aDirection);
};

/**
 * DefinitionListItemScanner() scans given `<dl>` element's children.
 * Then, you can check whether `<dt>` and/or `<dd>` elements are in it.
 */
class MOZ_STACK_CLASS DefinitionListItemScanner final {
  using Element = dom::Element;

 public:
  DefinitionListItemScanner() = delete;
  explicit DefinitionListItemScanner(Element& aDLElement) {
    MOZ_ASSERT(aDLElement.IsHTMLElement(nsGkAtoms::dl));
    for (nsIContent* child = aDLElement.GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsHTMLElement(nsGkAtoms::dt)) {
        mDTFound = true;
        if (mDDFound) {
          break;
        }
        continue;
      }
      if (child->IsHTMLElement(nsGkAtoms::dd)) {
        mDDFound = true;
        if (mDTFound) {
          break;
        }
        continue;
      }
    }
  }

  bool DTElementFound() const { return mDTFound; }
  bool DDElementFound() const { return mDDFound; }

 private:
  bool mDTFound = false;
  bool mDDFound = false;
};

/**
 * SelectedTableCellScanner() scans all table cell elements which are selected
 * by each selection range.  Note that if 2nd or later ranges do not select
 * only one table cell element, the ranges are just ignored.
 */
class MOZ_STACK_CLASS SelectedTableCellScanner final {
  using Element = dom::Element;
  using Selection = dom::Selection;

 public:
  SelectedTableCellScanner() = delete;
  explicit SelectedTableCellScanner(const Selection& aSelection) {
    Element* firstSelectedCellElement =
        HTMLEditUtils::GetFirstSelectedTableCellElement(aSelection);
    if (!firstSelectedCellElement) {
      return;  // We're not in table cell selection mode.
    }
    mSelectedCellElements.SetCapacity(aSelection.RangeCount());
    mSelectedCellElements.AppendElement(*firstSelectedCellElement);
    const uint32_t rangeCount = aSelection.RangeCount();
    for (const uint32_t i : IntegerRange(1u, rangeCount)) {
      MOZ_ASSERT(aSelection.RangeCount() == rangeCount);
      nsRange* range = aSelection.GetRangeAt(i);
      if (MOZ_UNLIKELY(NS_WARN_IF(!range)) ||
          MOZ_UNLIKELY(NS_WARN_IF(!range->IsPositioned()))) {
        continue;  // Shouldn't occur in normal conditions.
      }
      // Just ignore selection ranges which do not select only one table
      // cell element.  This is possible case if web apps sets multiple
      // selections and first range selects a table cell element.
      if (Element* selectedCellElement =
              HTMLEditUtils::GetTableCellElementIfOnlyOneSelected(*range)) {
        mSelectedCellElements.AppendElement(*selectedCellElement);
      }
    }
  }

  explicit SelectedTableCellScanner(const AutoRangeArray& aRanges) {
    if (aRanges.Ranges().IsEmpty()) {
      return;
    }
    Element* firstSelectedCellElement =
        HTMLEditUtils::GetTableCellElementIfOnlyOneSelected(
            aRanges.FirstRangeRef());
    if (!firstSelectedCellElement) {
      return;  // We're not in table cell selection mode.
    }
    mSelectedCellElements.SetCapacity(aRanges.Ranges().Length());
    mSelectedCellElements.AppendElement(*firstSelectedCellElement);
    for (uint32_t i = 1; i < aRanges.Ranges().Length(); i++) {
      nsRange* range = aRanges.Ranges()[i];
      if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
        continue;  // Shouldn't occur in normal conditions.
      }
      // Just ignore selection ranges which do not select only one table
      // cell element.  This is possible case if web apps sets multiple
      // selections and first range selects a table cell element.
      if (Element* selectedCellElement =
              HTMLEditUtils::GetTableCellElementIfOnlyOneSelected(*range)) {
        mSelectedCellElements.AppendElement(*selectedCellElement);
      }
    }
  }

  bool IsInTableCellSelectionMode() const {
    return !mSelectedCellElements.IsEmpty();
  }

  const nsTArray<OwningNonNull<Element>>& ElementsRef() const {
    return mSelectedCellElements;
  }

  /**
   * GetFirstElement() and GetNextElement() are stateful iterator methods.
   * This is useful to port legacy code which used old `nsITableEditor` API.
   */
  Element* GetFirstElement() const {
    MOZ_ASSERT(!mSelectedCellElements.IsEmpty());
    mIndex = 0;
    return !mSelectedCellElements.IsEmpty() ? mSelectedCellElements[0].get()
                                            : nullptr;
  }
  Element* GetNextElement() const {
    MOZ_ASSERT(mIndex < mSelectedCellElements.Length());
    return ++mIndex < mSelectedCellElements.Length()
               ? mSelectedCellElements[mIndex].get()
               : nullptr;
  }

 private:
  AutoTArray<OwningNonNull<Element>, 16> mSelectedCellElements;
  mutable size_t mIndex = 0;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_HTMLEditUtils_h
