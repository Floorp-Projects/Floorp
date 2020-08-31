/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditUtils_h
#define HTMLEditUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"
#include "nsCRT.h"
#include "nsGkAtoms.h"
#include "nsHTMLTags.h"

class nsAtom;

namespace mozilla {

enum class EditAction;

class HTMLEditUtils final {
  using Element = dom::Element;
  using Selection = dom::Selection;

 public:
  static const char16_t kSpace = 0x0020;
  static const char16_t kNBSP = 0x00A0;

  /**
   * IsSimplyEditableNode() returns true when aNode is simply editable.
   * This does NOT means that aNode can be removed from current parent nor
   * aNode's data is editable.
   */
  static bool IsSimplyEditableNode(const nsINode& aNode) {
    return aNode.IsEditable();
  }

  /**
   * IsRemovableFromParentNode() returns true when aContent is editable, has a
   * parent node and the parent node is also editable.
   */
  static bool IsRemovableFromParentNode(const nsIContent& aContent) {
    return aContent.IsEditable() && aContent.GetParentNode() &&
           aContent.GetParentNode()->IsEditable();
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
   * XXX This looks odd.  For example, how about a comment node?
   */
  static bool IsInlineElement(const nsIContent& aContent) {
    return !IsBlockElement(aContent);
  }

  static bool IsInlineStyle(nsINode* aNode);
  /**
   * IsRemovableInlineStyleElement() returns true if aElement is an inline
   * element and can be removed or split to in order to modifying inline
   * styles.
   */
  static bool IsRemovableInlineStyleElement(dom::Element& aElement);
  static bool IsFormatNode(nsINode* aNode);
  static bool IsNodeThatCanOutdent(nsINode* aNode);
  static bool IsHeader(nsINode& aNode);
  static bool IsListItem(nsINode* aNode);
  static bool IsTable(nsINode* aNode);
  static bool IsTableRow(nsINode* aNode);
  static bool IsAnyTableElement(nsINode* aNode);
  static bool IsAnyTableElementButNotTable(nsINode* aNode);
  static bool IsTableCell(nsINode* node);
  static bool IsTableCellOrCaption(nsINode& aNode);
  static bool IsAnyListElement(nsINode* aNode);
  static bool IsPre(nsINode* aNode);
  static bool IsImage(nsINode* aNode);
  static bool IsLink(nsINode* aNode);
  static bool IsNamedAnchor(nsINode* aNode);
  static bool IsMozDiv(nsINode* aNode);
  static bool IsMailCite(nsINode* aNode);
  static bool IsFormWidget(nsINode* aNode);
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
   * See execCommand spec:
   * https://w3c.github.io/editing/execCommand.html#non-list-single-line-container
   * https://w3c.github.io/editing/execCommand.html#single-line-container
   */
  static bool IsNonListSingleLineContainer(nsINode& aNode);
  static bool IsSingleLineContainer(nsINode& aNode);

  /**
   * GetLastLeafChild() returns rightmost leaf content in aNode.  It depends on
   * aChildBlockBoundary whether this scans into a block child or treat
   * block as a leaf.
   */
  enum class ChildBlockBoundary {
    // Even if there is a child block, keep scanning a leaf content in it.
    Ignore,
    // If there is a child block, return it.
    TreatAsLeaf,
  };
  static nsIContent* GetLastLeafChild(nsINode& aNode,
                                      ChildBlockBoundary aChildBlockBoundary) {
    for (nsIContent* content = aNode.GetLastChild(); content;
         content = content->GetLastChild()) {
      if (aChildBlockBoundary == ChildBlockBoundary::TreatAsLeaf &&
          HTMLEditUtils::IsBlockElement(*content)) {
        return content;
      }
      if (!content->HasChildren()) {
        return content;
      }
    }
    return nullptr;
  }

  /**
   * GetFirstLeafChild() returns leftmost leaf content in aNode.  It depends on
   * aChildBlockBoundary whether this scans into a block child or treat
   * block as a leaf.
   */
  static nsIContent* GetFirstLeafChild(nsINode& aNode,
                                       ChildBlockBoundary aChildBlockBoundary) {
    for (nsIContent* content = aNode.GetFirstChild(); content;
         content = content->GetFirstChild()) {
      if (aChildBlockBoundary == ChildBlockBoundary::TreatAsLeaf &&
          HTMLEditUtils::IsBlockElement(*content)) {
        return content;
      }
      if (!content->HasChildren()) {
        return content;
      }
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
   * @param aAncestorLimiter    Optional, setting this guarantees the
   *                            result is in aAncestorLimiter unless
   *                            aStartContent is not a descendant of this.
   */
  static nsIContent* GetNextLeafContentOrNextBlockElement(
      const nsIContent& aStartContent, const nsIContent& aCurrentBlock,
      const Element* aAncestorLimiter = nullptr) {
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
    if (HTMLEditUtils::IsContainerNode(*nextContent)) {
      // Else if it's a container, get deep leftmost child
      if (nsIContent* child = HTMLEditUtils::GetFirstLeafChild(
              *nextContent, ChildBlockBoundary::Ignore)) {
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
      const nsIContent& aCurrentBlock,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(aStartPoint.IsSet());

    if (!aStartPoint.IsInContentNode()) {
      return nullptr;
    }
    if (aStartPoint.IsInTextNode()) {
      return HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *aStartPoint.ContainerAsText(), aCurrentBlock, aAncestorLimiter);
    }
    if (!HTMLEditUtils::IsContainerNode(*aStartPoint.ContainerAsContent())) {
      return HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aAncestorLimiter);
    }

    nsCOMPtr<nsIContent> nextContent = aStartPoint.GetChild();
    if (!nextContent) {
      if (aStartPoint.GetContainer() == &aCurrentBlock) {
        // We are at end of the block.
        return nullptr;
      }

      // We are at end of non-block container
      return HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aAncestorLimiter);
    }

    // We have a next node.  If it's a block, return it.
    if (HTMLEditUtils::IsBlockElement(*nextContent)) {
      return nextContent;
    }
    if (HTMLEditUtils::IsContainerNode(*nextContent)) {
      // else if it's a container, get deep leftmost child
      if (nsIContent* child = HTMLEditUtils::GetFirstLeafChild(
              *nextContent, ChildBlockBoundary::Ignore)) {
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
   * @param aAncestorLimiter    Optional, setting this guarantees the
   *                            result is in aAncestorLimiter unless
   *                            aStartContent is not a descendant of this.
   */
  static nsIContent* GetPreviousLeafContentOrPreviousBlockElement(
      const nsIContent& aStartContent, const nsIContent& aCurrentBlock,
      const Element* aAncestorLimiter = nullptr) {
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
    if (HTMLEditUtils::IsContainerNode(*previousContent)) {
      // Else if it's a container, get deep rightmost child
      if (nsIContent* child = HTMLEditUtils::GetLastLeafChild(
              *previousContent, ChildBlockBoundary::Ignore)) {
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
      const nsIContent& aCurrentBlock,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(aStartPoint.IsSet());

    if (!aStartPoint.IsInContentNode()) {
      return nullptr;
    }
    if (aStartPoint.IsInTextNode()) {
      return HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          *aStartPoint.ContainerAsText(), aCurrentBlock, aAncestorLimiter);
    }
    if (!HTMLEditUtils::IsContainerNode(*aStartPoint.ContainerAsContent())) {
      return HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aAncestorLimiter);
    }

    if (aStartPoint.IsStartOfContainer()) {
      if (aStartPoint.GetContainer() == &aCurrentBlock) {
        // We are at start of the block.
        return nullptr;
      }

      // We are at start of non-block container
      return HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          *aStartPoint.ContainerAsContent(), aCurrentBlock, aAncestorLimiter);
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
    if (HTMLEditUtils::IsContainerNode(*previousContent)) {
      // Else if it's a container, get deep rightmost child
      if (nsIContent* child = HTMLEditUtils::GetLastLeafChild(
              *previousContent, ChildBlockBoundary::Ignore)) {
        return child;
      }
    }
    // Else return the node itself
    return previousContent;
  }

  /**
   * GetAncestorBlockElement() returns parent or nearest ancestor of aContent
   * which is a block element.  If aAncestorLimiter is not nullptr,
   * this stops looking for the result when it meets the limiter.
   */
  static Element* GetAncestorBlockElement(
      const nsIContent& aContent, const nsINode* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(
        !aAncestorLimiter || aContent.IsInclusiveDescendantOf(aAncestorLimiter),
        "aContent isn't in aAncestorLimiter");

    // The caller has already reached the limiter.
    if (&aContent == aAncestorLimiter) {
      return nullptr;
    }

    for (Element* element : aContent.AncestorsOfType<Element>()) {
      if (HTMLEditUtils::IsBlockElement(*element)) {
        return element;
      }
      // Now, we have reached the limiter, there is no block in its ancestors.
      if (element == aAncestorLimiter) {
        return nullptr;
      }
    }

    return nullptr;
  }

  /**
   * GetInclusiveAncestorBlockElement() returns aContent itself, or parent or
   * nearest ancestor of aContent which is a block element.  If aAncestorLimiter
   * is not nullptr, this stops looking for the result when it meets the
   * limiter.
   */
  static Element* GetInclusiveAncestorBlockElement(
      const nsIContent& aContent, const nsINode* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(
        !aAncestorLimiter || aContent.IsInclusiveDescendantOf(aAncestorLimiter),
        "aContent isn't in aAncestorLimiter");

    if (!aContent.IsContent()) {
      return nullptr;
    }

    if (HTMLEditUtils::IsBlockElement(aContent)) {
      return const_cast<Element*>(aContent.AsElement());
    }
    return GetAncestorBlockElement(aContent, aAncestorLimiter);
  }

  /**
   * GetInclusiveAncestorBlockElementExceptHRElement() returns inclusive
   * ancestor block element except `<hr>` element.
   */
  static Element* GetInclusiveAncestorBlockElementExceptHRElement(
      const nsIContent& aContent, const nsINode* aAncestorLimiter = nullptr) {
    Element* blockElement =
        GetInclusiveAncestorBlockElement(aContent, aAncestorLimiter);
    if (!blockElement || !blockElement->IsHTMLElement(nsGkAtoms::hr)) {
      return blockElement;
    }
    if (!blockElement->GetParentElement()) {
      return nullptr;
    }
    return GetInclusiveAncestorBlockElementExceptHRElement(
        *blockElement->GetParentElement(), aAncestorLimiter);
  }

  /**
   * GetInclusiveAncestorEditableBlockElementOrInlineEditingHost() returns
   * inclusive block ancestor element of aContent.  If aContent is in inline
   * editing host, returns the editing host instead.
   */
  static Element* GetInclusiveAncestorEditableBlockElementOrInlineEditingHost(
      nsIContent& aContent);
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

  static Element* GetClosestAncestorAnyListElement(const nsIContent& aContent);

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
  static Element* GetElementIfOnlyOneSelected(
      const dom::AbstractRange& aRange) {
    if (!aRange.IsPositioned()) {
      return nullptr;
    }
    const RangeBoundary& start = aRange.StartRef();
    const RangeBoundary& end = aRange.EndRef();
    if (NS_WARN_IF(!start.IsSetAndValid()) ||
        NS_WARN_IF(!end.IsSetAndValid()) ||
        start.Container() != end.Container()) {
      return nullptr;
    }
    nsIContent* childAtStart = start.GetChildAtOffset();
    if (!childAtStart || !childAtStart->IsElement()) {
      return nullptr;
    }
    // If start child is not the last sibling and only if end child is its
    // next sibling, the start child is selected.
    if (childAtStart->GetNextSibling()) {
      return childAtStart->GetNextSibling() == end.GetChildAtOffset()
                 ? childAtStart->AsElement()
                 : nullptr;
    }
    // If start child is the last sibling and only if no child at the end,
    // the start child is selected.
    return !end.GetChildAtOffset() ? childAtStart->AsElement() : nullptr;
  }

  static Element* GetTableCellElementIfOnlyOneSelected(
      const dom::AbstractRange& aRange) {
    Element* element = HTMLEditUtils::GetElementIfOnlyOneSelected(aRange);
    return element && HTMLEditUtils::IsTableCell(element) ? element : nullptr;
  }

  static EditAction GetEditActionForInsert(const nsAtom& aTagName);
  static EditAction GetEditActionForRemoveList(const nsAtom& aTagName);
  static EditAction GetEditActionForInsert(const Element& aElement);
  static EditAction GetEditActionForFormatText(const nsAtom& aProperty,
                                               const nsAtom* aAttribute,
                                               bool aToSetStyle);
  static EditAction GetEditActionForAlignment(const nsAString& aAlignType);

  /**
   * GetPreviousCharOffsetExceptASCIIWhiteSpace() returns offset of previous
   * character which is not ASCII white-space characters.
   */
  static Maybe<uint32_t> GetPreviousCharOffsetExceptASCIIWhiteSpaces(
      const EditorDOMPointInText& aPoint) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetPreviousCharOffsetExceptASCIIWhiteSpaces(
        *aPoint.ContainerAsText(), aPoint.Offset());
  }
  static Maybe<uint32_t> GetPreviousCharOffsetExceptASCIIWhiteSpaces(
      const dom::Text& aTextNode, uint32_t aOffset) {
    const nsTextFragment& textFragment = aTextNode.TextFragment();
    MOZ_ASSERT(aOffset <= textFragment.GetLength());
    for (uint32_t i = aOffset; i; i--) {
      if (!nsCRT::IsAsciiSpace(textFragment.CharAt(i - 1))) {
        return Some(i - 1);
      }
    }
    return Nothing();
  }

  /**
   * GetNextCharOffsetExceptASCIIWhiteSpace() returns offset of next character
   * which is not ASCII white-space characters.
   */
  static Maybe<uint32_t> GetNextCharOffsetExceptASCIIWhiteSpaces(
      const EditorDOMPointInText& aPoint) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetNextCharOffsetExceptASCIIWhiteSpaces(*aPoint.ContainerAsText(),
                                                   aPoint.Offset());
  }
  static Maybe<uint32_t> GetNextCharOffsetExceptASCIIWhiteSpaces(
      const dom::Text& aTextNode, uint32_t aOffset) {
    const nsTextFragment& textFragment = aTextNode.TextFragment();
    MOZ_ASSERT(aOffset <= textFragment.GetLength());
    for (uint32_t i = aOffset + 1; i < textFragment.GetLength(); i++) {
      if (!nsCRT::IsAsciiSpace(textFragment.CharAt(i))) {
        return Some(i);
      }
    }
    return Nothing();
  }

  /**
   * GetPreviousCharOffsetExceptWhiteSpaces() returns first offset where
   * the character is neither an ASCII white-space nor an NBSP before aPoint.
   */
  static Maybe<uint32_t> GetPreviousCharOffsetExceptWhiteSpaces(
      const EditorDOMPointInText& aPoint) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetPreviousCharOffsetExceptWhiteSpaces(*aPoint.ContainerAsText(),
                                                  aPoint.Offset());
  }
  static Maybe<uint32_t> GetPreviousCharOffsetExceptWhiteSpaces(
      const dom::Text& aTextNode, uint32_t aOffset) {
    if (!aOffset) {
      return Nothing();
    }
    const nsTextFragment& textFragment = aTextNode.TextFragment();
    MOZ_ASSERT(aOffset <= textFragment.GetLength());
    for (uint32_t i = aOffset; i; i--) {
      char16_t ch = textFragment.CharAt(i - 1);
      if (!nsCRT::IsAsciiSpace(ch) && ch != kNBSP) {
        return Some(i - 1);
      }
    }
    return Nothing();
  }

  /**
   * GetInclusiveNextCharOffsetExceptWhiteSpaces() returns first offset where
   * the character is neither an ASCII white-space nor an NBSP at aPoint or
   * after it.
   */
  static Maybe<uint32_t> GetInclusiveNextCharOffsetExceptWhiteSpaces(
      const EditorDOMPointInText& aPoint) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    return GetInclusiveNextCharOffsetExceptWhiteSpaces(
        *aPoint.ContainerAsText(), aPoint.Offset());
  }
  static Maybe<uint32_t> GetInclusiveNextCharOffsetExceptWhiteSpaces(
      const dom::Text& aTextNode, uint32_t aOffset) {
    const nsTextFragment& textFragment = aTextNode.TextFragment();
    MOZ_ASSERT(aOffset <= textFragment.GetLength());
    for (uint32_t i = aOffset; i < textFragment.GetLength(); i++) {
      char16_t ch = textFragment.CharAt(i);
      if (!nsCRT::IsAsciiSpace(ch) && ch != kNBSP) {
        return Some(i);
      }
    }
    return Nothing();
  }

  /**
   * GetFirstASCIIWhiteSpaceOffsetCollapsedWith() returns first ASCII
   * white-space offset which is collapsed with a white-space at the given
   * position.  I.e., the character at the position must be an ASCII
   * white-space.
   */
  static uint32_t GetFirstASCIIWhiteSpaceOffsetCollapsedWith(
      const EditorDOMPointInText& aPoint) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    MOZ_ASSERT(!aPoint.IsEndOfContainer());
    MOZ_ASSERT(aPoint.IsCharASCIISpace());
    return GetFirstASCIIWhiteSpaceOffsetCollapsedWith(*aPoint.ContainerAsText(),
                                                      aPoint.Offset());
  }
  static uint32_t GetFirstASCIIWhiteSpaceOffsetCollapsedWith(
      const dom::Text& aTextNode, uint32_t aOffset) {
    MOZ_ASSERT(aOffset < aTextNode.TextLength());
    MOZ_ASSERT(nsCRT::IsAsciiSpace(aTextNode.TextFragment().CharAt(aOffset)));
    if (!aOffset) {
      return 0;
    }
    Maybe<uint32_t> previousVisibleCharOffset =
        GetPreviousCharOffsetExceptASCIIWhiteSpaces(aTextNode, aOffset);
    return previousVisibleCharOffset.isSome()
               ? previousVisibleCharOffset.value() + 1
               : 0;
  }

 private:
  static bool CanNodeContain(nsHTMLTag aParentTagId, nsHTMLTag aChildTagId);
  static bool IsContainerNode(nsHTMLTag aTagId);
};

/**
 * DefinitionListItemScanner() scans given `<dl>` element's children.
 * Then, you can check whether `<dt>` and/or `<dd>` elements are in it.
 */
class MOZ_STACK_CLASS DefinitionListItemScanner final {
 public:
  DefinitionListItemScanner() = delete;
  explicit DefinitionListItemScanner(dom::Element& aDLElement) {
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

}  // namespace mozilla

#endif  // #ifndef HTMLEditUtils_h
