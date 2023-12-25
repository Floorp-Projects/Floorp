/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditUtils.h"

#include "AutoRangeArray.h"   // for AutoRangeArray
#include "CSSEditUtils.h"     // for CSSEditUtils
#include "EditAction.h"       // for EditAction
#include "EditorBase.h"       // for EditorBase, EditorType
#include "EditorDOMPoint.h"   // for EditorDOMPoint, etc.
#include "EditorForwards.h"   // for CollectChildrenOptions
#include "EditorUtils.h"      // for EditorUtils
#include "HTMLEditHelpers.h"  // for EditorInlineStyle
#include "WSRunObject.h"      // for WSRunScanner

#include "mozilla/ArrayUtils.h"  // for ArrayLength
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc.
#include "mozilla/Attributes.h"
#include "mozilla/StaticPrefs_editor.h"   // for StaticPrefs::editor_
#include "mozilla/RangeUtils.h"           // for RangeUtils
#include "mozilla/dom/DocumentInlines.h"  // for GetBodyElement()
#include "mozilla/dom/Element.h"          // for Element, nsINode
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/ServoCSSParser.h"  // for ServoCSSParser
#include "mozilla/dom/StaticRange.h"
#include "mozilla/dom/Text.h"  // for Text

#include "nsAString.h"    // for nsAString::IsEmpty
#include "nsAtom.h"       // for nsAtom
#include "nsAttrValue.h"  // nsAttrValue
#include "nsCaseTreatment.h"
#include "nsCOMPtr.h"            // for nsCOMPtr, operator==, etc.
#include "nsComputedDOMStyle.h"  // for nsComputedDOMStyle
#include "nsDebug.h"             // for NS_ASSERTION, etc.
#include "nsElementTable.h"      // for nsHTMLElement
#include "nsError.h"             // for NS_SUCCEEDED
#include "nsGkAtoms.h"           // for nsGkAtoms, nsGkAtoms::a, etc.
#include "nsHTMLTags.h"
#include "nsIFrameInlines.h"     // for nsIFrame::IsFlexOrGridItem()
#include "nsLiteralString.h"     // for NS_LITERAL_STRING
#include "nsNameSpaceManager.h"  // for kNameSpaceID_None
#include "nsPrintfCString.h"     // nsPringfCString
#include "nsString.h"            // for nsAutoString
#include "nsStyledElement.h"
#include "nsStyleStruct.h"   // for StyleDisplay
#include "nsStyleUtil.h"     // for nsStyleUtil
#include "nsTextFragment.h"  // for nsTextFragment
#include "nsTextFrame.h"     // for nsTextFrame

namespace mozilla {

using namespace dom;
using EditorType = EditorBase::EditorType;

template nsIContent* HTMLEditUtils::GetPreviousContent(
    const EditorDOMPoint& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetPreviousContent(
    const EditorRawDOMPoint& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetPreviousContent(
    const EditorDOMPointInText& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetPreviousContent(
    const EditorRawDOMPointInText& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetNextContent(
    const EditorDOMPoint& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetNextContent(
    const EditorRawDOMPoint& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetNextContent(
    const EditorDOMPointInText& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template nsIContent* HTMLEditUtils::GetNextContent(
    const EditorRawDOMPointInText& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);

template EditorDOMPoint HTMLEditUtils::GetPreviousEditablePoint(
    nsIContent& aContent, const Element* aAncestorLimiter,
    InvisibleWhiteSpaces aInvisibleWhiteSpaces,
    TableBoundary aHowToTreatTableBoundary);
template EditorRawDOMPoint HTMLEditUtils::GetPreviousEditablePoint(
    nsIContent& aContent, const Element* aAncestorLimiter,
    InvisibleWhiteSpaces aInvisibleWhiteSpaces,
    TableBoundary aHowToTreatTableBoundary);
template EditorDOMPoint HTMLEditUtils::GetNextEditablePoint(
    nsIContent& aContent, const Element* aAncestorLimiter,
    InvisibleWhiteSpaces aInvisibleWhiteSpaces,
    TableBoundary aHowToTreatTableBoundary);
template EditorRawDOMPoint HTMLEditUtils::GetNextEditablePoint(
    nsIContent& aContent, const Element* aAncestorLimiter,
    InvisibleWhiteSpaces aInvisibleWhiteSpaces,
    TableBoundary aHowToTreatTableBoundary);

template nsIContent* HTMLEditUtils::GetContentToPreserveInlineStyles(
    const EditorDOMPoint& aPoint, const Element& aEditingHost);
template nsIContent* HTMLEditUtils::GetContentToPreserveInlineStyles(
    const EditorRawDOMPoint& aPoint, const Element& aEditingHost);

template EditorDOMPoint HTMLEditUtils::GetBetterInsertionPointFor(
    const nsIContent& aContentToInsert, const EditorDOMPoint& aPointToInsert,
    const Element& aEditingHost);
template EditorRawDOMPoint HTMLEditUtils::GetBetterInsertionPointFor(
    const nsIContent& aContentToInsert, const EditorRawDOMPoint& aPointToInsert,
    const Element& aEditingHost);
template EditorDOMPoint HTMLEditUtils::GetBetterInsertionPointFor(
    const nsIContent& aContentToInsert, const EditorRawDOMPoint& aPointToInsert,
    const Element& aEditingHost);
template EditorRawDOMPoint HTMLEditUtils::GetBetterInsertionPointFor(
    const nsIContent& aContentToInsert, const EditorDOMPoint& aPointToInsert,
    const Element& aEditingHost);

template EditorDOMPoint HTMLEditUtils::GetBetterCaretPositionToInsertText(
    const EditorDOMPoint& aPoint, const Element& aEditingHost);
template EditorDOMPoint HTMLEditUtils::GetBetterCaretPositionToInsertText(
    const EditorRawDOMPoint& aPoint, const Element& aEditingHost);
template EditorRawDOMPoint HTMLEditUtils::GetBetterCaretPositionToInsertText(
    const EditorDOMPoint& aPoint, const Element& aEditingHost);
template EditorRawDOMPoint HTMLEditUtils::GetBetterCaretPositionToInsertText(
    const EditorRawDOMPoint& aPoint, const Element& aEditingHost);

template Result<EditorDOMPoint, nsresult>
HTMLEditUtils::ComputePointToPutCaretInElementIfOutside(
    const Element& aElement, const EditorDOMPoint& aCurrentPoint);
template Result<EditorRawDOMPoint, nsresult>
HTMLEditUtils::ComputePointToPutCaretInElementIfOutside(
    const Element& aElement, const EditorDOMPoint& aCurrentPoint);
template Result<EditorDOMPoint, nsresult>
HTMLEditUtils::ComputePointToPutCaretInElementIfOutside(
    const Element& aElement, const EditorRawDOMPoint& aCurrentPoint);
template Result<EditorRawDOMPoint, nsresult>
HTMLEditUtils::ComputePointToPutCaretInElementIfOutside(
    const Element& aElement, const EditorRawDOMPoint& aCurrentPoint);

template bool HTMLEditUtils::IsSameCSSColorValue(const nsAString& aColorA,
                                                 const nsAString& aColorB);
template bool HTMLEditUtils::IsSameCSSColorValue(const nsACString& aColorA,
                                                 const nsACString& aColorB);

bool HTMLEditUtils::CanContentsBeJoined(const nsIContent& aLeftContent,
                                        const nsIContent& aRightContent) {
  if (aLeftContent.NodeInfo()->NameAtom() !=
      aRightContent.NodeInfo()->NameAtom()) {
    return false;
  }

  if (!aLeftContent.IsElement()) {
    return true;  // can join text nodes, etc
  }
  MOZ_ASSERT(aRightContent.IsElement());

  if (aLeftContent.NodeInfo()->NameAtom() == nsGkAtoms::font) {
    const nsAttrValue* const leftSize =
        aLeftContent.AsElement()->GetParsedAttr(nsGkAtoms::size);
    const nsAttrValue* const rightSize =
        aRightContent.AsElement()->GetParsedAttr(nsGkAtoms::size);
    if (!leftSize ^ !rightSize || (leftSize && !leftSize->Equals(*rightSize))) {
      return false;
    }

    const nsAttrValue* const leftColor =
        aLeftContent.AsElement()->GetParsedAttr(nsGkAtoms::color);
    const nsAttrValue* const rightColor =
        aRightContent.AsElement()->GetParsedAttr(nsGkAtoms::color);
    if (!leftColor ^ !rightColor ||
        (leftColor && !leftColor->Equals(*rightColor))) {
      return false;
    }

    const nsAttrValue* const leftFace =
        aLeftContent.AsElement()->GetParsedAttr(nsGkAtoms::face);
    const nsAttrValue* const rightFace =
        aRightContent.AsElement()->GetParsedAttr(nsGkAtoms::face);
    if (!leftFace ^ !rightFace || (leftFace && !leftFace->Equals(*rightFace))) {
      return false;
    }
  }
  nsStyledElement* leftStyledElement =
      nsStyledElement::FromNode(const_cast<nsIContent*>(&aLeftContent));
  if (!leftStyledElement) {
    return false;
  }
  nsStyledElement* rightStyledElement =
      nsStyledElement::FromNode(const_cast<nsIContent*>(&aRightContent));
  if (!rightStyledElement) {
    return false;
  }
  return CSSEditUtils::DoStyledElementsHaveSameStyle(*leftStyledElement,
                                                     *rightStyledElement);
}

static bool IsHTMLBlockElementByDefault(const nsIContent& aContent) {
  if (!aContent.IsHTMLElement()) {
    return false;
  }
  if (aContent.IsHTMLElement(nsGkAtoms::br)) {  // shortcut for TextEditor
    MOZ_ASSERT(!nsHTMLElement::IsBlock(
        nsHTMLTags::CaseSensitiveAtomTagToId(nsGkAtoms::br)));
    return false;
  }
  // We want to treat these as block nodes even though nsHTMLElement says
  // they're not.
  if (aContent.IsAnyOfHTMLElements(
          nsGkAtoms::body, nsGkAtoms::head, nsGkAtoms::tbody, nsGkAtoms::thead,
          nsGkAtoms::tfoot, nsGkAtoms::tr, nsGkAtoms::th, nsGkAtoms::td,
          nsGkAtoms::dt, nsGkAtoms::dd)) {
    return true;
  }

  return nsHTMLElement::IsBlock(
      nsHTMLTags::CaseSensitiveAtomTagToId(aContent.NodeInfo()->NameAtom()));
}

bool HTMLEditUtils::IsBlockElement(const nsIContent& aContent,
                                   BlockInlineCheck aBlockInlineCheck) {
  MOZ_ASSERT(aBlockInlineCheck != BlockInlineCheck::Unused);

  if (MOZ_UNLIKELY(!aContent.IsElement())) {
    return false;
  }
  if (!StaticPrefs::editor_block_inline_check_use_computed_style() ||
      aBlockInlineCheck == BlockInlineCheck::UseHTMLDefaultStyle) {
    return IsHTMLBlockElementByDefault(aContent);
  }
  // Let's treat the document element and the body element is a block to avoid
  // complicated things which may be detected by fuzzing.
  if (aContent.OwnerDoc()->GetDocumentElement() == &aContent ||
      (aContent.IsHTMLElement(nsGkAtoms::body) &&
       aContent.OwnerDoc()->GetBodyElement() == &aContent)) {
    return true;
  }
  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aContent.AsElement());
  if (MOZ_UNLIKELY(!elementStyle)) {  // If aContent is not in the composed tree
    return IsHTMLBlockElementByDefault(aContent);
  }
  const nsStyleDisplay* styleDisplay = elementStyle->StyleDisplay();
  if (MOZ_UNLIKELY(styleDisplay->mDisplay == StyleDisplay::None)) {
    // Typically, we should not keep handling editing in invisible nodes, but if
    // we reach here, let's fallback to the default style for protecting the
    // structure as far as possible.
    return IsHTMLBlockElementByDefault(aContent);
  }
  // Both Blink and WebKit treat ruby style as a block, see IsEnclosingBlock()
  // in Chromium or isBlock() in WebKit.
  if (styleDisplay->IsRubyDisplayType()) {
    return true;
  }
  // If the outside is not inline, treat it as block.
  if (!styleDisplay->IsInlineOutsideStyle()) {
    return true;
  }
  // If we're checking display-inside, inline-block, etc should be a block too.
  return aBlockInlineCheck == BlockInlineCheck::UseComputedDisplayStyle &&
         styleDisplay->DisplayInside() == StyleDisplayInside::FlowRoot &&
         // Treat widgets as inline since they won't hide collapsible
         // white-spaces around them.
         styleDisplay->EffectiveAppearance() == StyleAppearance::None;
}

bool HTMLEditUtils::IsInlineContent(const nsIContent& aContent,
                                    BlockInlineCheck aBlockInlineCheck) {
  MOZ_ASSERT(aBlockInlineCheck != BlockInlineCheck::Unused);

  if (!aContent.IsElement()) {
    return true;
  }
  if (!StaticPrefs::editor_block_inline_check_use_computed_style() ||
      aBlockInlineCheck == BlockInlineCheck::UseHTMLDefaultStyle) {
    return !IsHTMLBlockElementByDefault(aContent);
  }
  // Let's treat the document element and the body element is a block to avoid
  // complicated things which may be detected by fuzzing.
  if (aContent.OwnerDoc()->GetDocumentElement() == &aContent ||
      (aContent.IsHTMLElement(nsGkAtoms::body) &&
       aContent.OwnerDoc()->GetBodyElement() == &aContent)) {
    return false;
  }
  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aContent.AsElement());
  if (MOZ_UNLIKELY(!elementStyle)) {  // If aContent is not in the composed tree
    return !IsHTMLBlockElementByDefault(aContent);
  }
  const nsStyleDisplay* styleDisplay = elementStyle->StyleDisplay();
  if (MOZ_UNLIKELY(styleDisplay->mDisplay == StyleDisplay::None)) {
    // Similar to IsBlockElement, let's fallback to refer the default style.
    // Note that if you change here, you may need to check the parent element
    // style if aContent.
    return !IsHTMLBlockElementByDefault(aContent);
  }
  // Different block IsBlockElement, when the display-outside is inline, it's
  // simply an inline element.
  return styleDisplay->IsInlineOutsideStyle() ||
         styleDisplay->IsRubyDisplayType();
}

bool HTMLEditUtils::IsFlexOrGridItem(const Element& aElement) {
  nsIFrame* frame = aElement.GetPrimaryFrame();
  return frame && frame->IsFlexOrGridItem();
}

bool HTMLEditUtils::IsInclusiveAncestorCSSDisplayNone(
    const nsIContent& aContent) {
  if (NS_WARN_IF(!aContent.IsInComposedDoc())) {
    return true;
  }
  for (const Element* element :
       aContent.InclusiveFlatTreeAncestorsOfType<Element>()) {
    RefPtr<const ComputedStyle> elementStyle =
        nsComputedDOMStyle::GetComputedStyleNoFlush(element);
    if (NS_WARN_IF(!elementStyle)) {
      continue;
    }
    const nsStyleDisplay* styleDisplay = elementStyle->StyleDisplay();
    if (MOZ_UNLIKELY(styleDisplay->mDisplay == StyleDisplay::None)) {
      return true;
    }
  }
  return false;
}

bool HTMLEditUtils::IsVisibleElementEvenIfLeafNode(const nsIContent& aContent) {
  if (!aContent.IsElement()) {
    return false;
  }
  // Assume non-HTML element is visible.
  if (!aContent.IsHTMLElement()) {
    return true;
  }
  // XXX Should we return false if the element is display:none?
  if (HTMLEditUtils::IsBlockElement(
          aContent, BlockInlineCheck::UseComputedDisplayStyle)) {
    return true;
  }
  if (aContent.IsAnyOfHTMLElements(nsGkAtoms::applet, nsGkAtoms::iframe,
                                   nsGkAtoms::img, nsGkAtoms::meter,
                                   nsGkAtoms::progress, nsGkAtoms::select,
                                   nsGkAtoms::textarea)) {
    return true;
  }
  if (const HTMLInputElement* inputElement =
          HTMLInputElement::FromNode(&aContent)) {
    return inputElement->ControlType() != FormControlType::InputHidden;
  }
  // Maybe, empty inline element such as <span>.
  return false;
}

/**
 * IsInlineStyle() returns true if aNode is an inline style.
 */
bool HTMLEditUtils::IsInlineStyle(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(
      nsGkAtoms::b, nsGkAtoms::i, nsGkAtoms::u, nsGkAtoms::tt, nsGkAtoms::s,
      nsGkAtoms::strike, nsGkAtoms::big, nsGkAtoms::small, nsGkAtoms::sub,
      nsGkAtoms::sup, nsGkAtoms::font);
}

bool HTMLEditUtils::IsDisplayOutsideInline(const Element& aElement) {
  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(&aElement);
  if (!elementStyle) {
    return false;
  }
  return elementStyle->StyleDisplay()->DisplayOutside() ==
         StyleDisplayOutside::Inline;
}

bool HTMLEditUtils::IsDisplayInsideFlowRoot(const Element& aElement) {
  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(&aElement);
  if (!elementStyle) {
    return false;
  }
  return elementStyle->StyleDisplay()->DisplayInside() ==
         StyleDisplayInside::FlowRoot;
}

bool HTMLEditUtils::IsRemovableInlineStyleElement(Element& aElement) {
  if (!aElement.IsHTMLElement()) {
    return false;
  }
  // https://w3c.github.io/editing/execCommand.html#removeformat-candidate
  if (aElement.IsAnyOfHTMLElements(
          nsGkAtoms::abbr,  // Chrome ignores, but does not make sense.
          nsGkAtoms::acronym, nsGkAtoms::b,
          nsGkAtoms::bdi,  // Chrome ignores, but does not make sense.
          nsGkAtoms::bdo, nsGkAtoms::big, nsGkAtoms::cite, nsGkAtoms::code,
          // nsGkAtoms::del, Chrome ignores, but does not make sense but
          // execCommand unofficial draft excludes this.  Spec issue:
          // https://github.com/w3c/editing/issues/192
          nsGkAtoms::dfn, nsGkAtoms::em, nsGkAtoms::font, nsGkAtoms::i,
          nsGkAtoms::ins, nsGkAtoms::kbd,
          nsGkAtoms::mark,  // Chrome ignores, but does not make sense.
          nsGkAtoms::nobr, nsGkAtoms::q, nsGkAtoms::s, nsGkAtoms::samp,
          nsGkAtoms::small, nsGkAtoms::span, nsGkAtoms::strike,
          nsGkAtoms::strong, nsGkAtoms::sub, nsGkAtoms::sup, nsGkAtoms::tt,
          nsGkAtoms::u, nsGkAtoms::var)) {
    return true;
  }
  // If it's a <blink> element, we can remove it.
  nsAutoString tagName;
  aElement.GetTagName(tagName);
  return tagName.LowerCaseEqualsASCII("blink");
}

/**
 * IsNodeThatCanOutdent() returns true if aNode is a list, list item or
 * blockquote.
 */
bool HTMLEditUtils::IsNodeThatCanOutdent(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::ul, nsGkAtoms::ol, nsGkAtoms::dl,
                                    nsGkAtoms::li, nsGkAtoms::dd, nsGkAtoms::dt,
                                    nsGkAtoms::blockquote);
}

/**
 * IsHeader() returns true if aNode is an html header.
 */
bool HTMLEditUtils::IsHeader(nsINode& aNode) {
  return aNode.IsAnyOfHTMLElements(nsGkAtoms::h1, nsGkAtoms::h2, nsGkAtoms::h3,
                                   nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6);
}

/**
 * IsListItem() returns true if aNode is an html list item.
 */
bool HTMLEditUtils::IsListItem(const nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::li, nsGkAtoms::dd,
                                    nsGkAtoms::dt);
}

/**
 * IsAnyTableElement() returns true if aNode is an html table, td, tr, ...
 */
bool HTMLEditUtils::IsAnyTableElement(const nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(
      nsGkAtoms::table, nsGkAtoms::tr, nsGkAtoms::td, nsGkAtoms::th,
      nsGkAtoms::thead, nsGkAtoms::tfoot, nsGkAtoms::tbody, nsGkAtoms::caption);
}

/**
 * IsAnyTableElementButNotTable() returns true if aNode is an html td, tr, ...
 * (doesn't include table)
 */
bool HTMLEditUtils::IsAnyTableElementButNotTable(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::tr, nsGkAtoms::td, nsGkAtoms::th,
                                    nsGkAtoms::thead, nsGkAtoms::tfoot,
                                    nsGkAtoms::tbody, nsGkAtoms::caption);
}

/**
 * IsTable() returns true if aNode is an html table.
 */
bool HTMLEditUtils::IsTable(nsINode* aNode) {
  return aNode && aNode->IsHTMLElement(nsGkAtoms::table);
}

/**
 * IsTableRow() returns true if aNode is an html tr.
 */
bool HTMLEditUtils::IsTableRow(nsINode* aNode) {
  return aNode && aNode->IsHTMLElement(nsGkAtoms::tr);
}

/**
 * IsTableCell() returns true if aNode is an html td or th.
 */
bool HTMLEditUtils::IsTableCell(const nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th);
}

/**
 * IsTableCellOrCaption() returns true if aNode is an html td or th or caption.
 */
bool HTMLEditUtils::IsTableCellOrCaption(nsINode& aNode) {
  return aNode.IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th,
                                   nsGkAtoms::caption);
}

/**
 * IsAnyListElement() returns true if aNode is an html list.
 */
bool HTMLEditUtils::IsAnyListElement(const nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::ul, nsGkAtoms::ol,
                                    nsGkAtoms::dl);
}

/**
 * IsPre() returns true if aNode is an html pre node.
 */
bool HTMLEditUtils::IsPre(const nsINode* aNode) {
  return aNode && aNode->IsHTMLElement(nsGkAtoms::pre);
}

/**
 * IsImage() returns true if aNode is an html image node.
 */
bool HTMLEditUtils::IsImage(nsINode* aNode) {
  return aNode && aNode->IsHTMLElement(nsGkAtoms::img);
}

bool HTMLEditUtils::IsLink(const nsINode* aNode) {
  MOZ_ASSERT(aNode);

  if (!aNode->IsContent()) {
    return false;
  }

  RefPtr<const dom::HTMLAnchorElement> anchor =
      dom::HTMLAnchorElement::FromNodeOrNull(aNode->AsContent());
  if (!anchor) {
    return false;
  }

  nsAutoString tmpText;
  anchor->GetHref(tmpText);
  return !tmpText.IsEmpty();
}

bool HTMLEditUtils::IsNamedAnchor(const nsINode* aNode) {
  MOZ_ASSERT(aNode);
  if (!aNode->IsHTMLElement(nsGkAtoms::a)) {
    return false;
  }

  nsAutoString text;
  return aNode->AsElement()->GetAttr(nsGkAtoms::name, text) && !text.IsEmpty();
}

/**
 * IsMozDiv() returns true if aNode is an html div node with |type = _moz|.
 */
bool HTMLEditUtils::IsMozDiv(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsHTMLElement(nsGkAtoms::div) &&
         aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                         u"_moz"_ns, eIgnoreCase);
}

/**
 * IsMailCite() returns true if aNode is an html blockquote with |type=cite|.
 */
bool HTMLEditUtils::IsMailCite(const Element& aElement) {
  // don't ask me why, but our html mailcites are id'd by "type=cite"...
  if (aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::type, u"cite"_ns,
                           eIgnoreCase)) {
    return true;
  }

  // ... but our plaintext mailcites by "_moz_quote=true".  go figure.
  if (aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::mozquote, u"true"_ns,
                           eIgnoreCase)) {
    return true;
  }

  return false;
}

/**
 * IsFormWidget() returns true if aNode is a form widget of some kind.
 */
bool HTMLEditUtils::IsFormWidget(const nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::textarea, nsGkAtoms::select,
                                    nsGkAtoms::button, nsGkAtoms::output,
                                    nsGkAtoms::progress, nsGkAtoms::meter,
                                    nsGkAtoms::input);
}

bool HTMLEditUtils::SupportsAlignAttr(nsINode& aNode) {
  return aNode.IsAnyOfHTMLElements(
      nsGkAtoms::hr, nsGkAtoms::table, nsGkAtoms::tbody, nsGkAtoms::tfoot,
      nsGkAtoms::thead, nsGkAtoms::tr, nsGkAtoms::td, nsGkAtoms::th,
      nsGkAtoms::div, nsGkAtoms::p, nsGkAtoms::h1, nsGkAtoms::h2, nsGkAtoms::h3,
      nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6);
}

bool HTMLEditUtils::IsVisibleTextNode(const Text& aText) {
  if (!aText.TextDataLength()) {
    return false;
  }

  Maybe<uint32_t> visibleCharOffset =
      HTMLEditUtils::GetInclusiveNextNonCollapsibleCharOffset(
          EditorDOMPointInText(&aText, 0));
  if (visibleCharOffset.isSome()) {
    return true;
  }

  // Now, all characters in aText is collapsible white-spaces.  The node is
  // invisible if next to block boundary.
  return !HTMLEditUtils::GetElementOfImmediateBlockBoundary(
             aText, WalkTreeDirection::Forward) &&
         !HTMLEditUtils::GetElementOfImmediateBlockBoundary(
             aText, WalkTreeDirection::Backward);
}

bool HTMLEditUtils::IsInVisibleTextFrames(nsPresContext* aPresContext,
                                          const Text& aText) {
  // TODO(dholbert): aPresContext is now unused; maybe we can remove it, here
  // and in IsEmptyNode?  We do use it as a signal (implicitly here,
  // more-explicitly in IsEmptyNode) that we are in a "SafeToAskLayout" case...
  // If/when we remove it, we should be sure we're not losing that signal of
  // strictness, since this function here does absolutely need to query layout.
  MOZ_ASSERT(aPresContext);

  if (!aText.TextDataLength()) {
    return false;
  }

  nsTextFrame* textFrame = do_QueryFrame(aText.GetPrimaryFrame());
  if (!textFrame) {
    return false;
  }

  return textFrame->HasVisibleText();
}

Element* HTMLEditUtils::GetElementOfImmediateBlockBoundary(
    const nsIContent& aContent, const WalkTreeDirection aDirection) {
  MOZ_ASSERT(aContent.IsHTMLElement(nsGkAtoms::br) || aContent.IsText());

  // First, we get a block container.  This is not designed for reaching
  // no block boundaries in the tree.
  Element* maybeNonEditableAncestorBlock = HTMLEditUtils::GetAncestorElement(
      aContent, HTMLEditUtils::ClosestBlockElement,
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!maybeNonEditableAncestorBlock)) {
    return nullptr;
  }

  auto getNextContent = [&aDirection, &maybeNonEditableAncestorBlock](
                            const nsIContent& aContent) -> nsIContent* {
    return aDirection == WalkTreeDirection::Forward
               ? HTMLEditUtils::GetNextContent(
                     aContent,
                     {WalkTreeOption::IgnoreDataNodeExceptText,
                      WalkTreeOption::StopAtBlockBoundary},
                     BlockInlineCheck::UseComputedDisplayStyle,
                     maybeNonEditableAncestorBlock)
               : HTMLEditUtils::GetPreviousContent(
                     aContent,
                     {WalkTreeOption::IgnoreDataNodeExceptText,
                      WalkTreeOption::StopAtBlockBoundary},
                     BlockInlineCheck::UseComputedDisplayStyle,
                     maybeNonEditableAncestorBlock);
  };

  // Then, scan block element boundary while we don't see visible things.
  const bool isBRElement = aContent.IsHTMLElement(nsGkAtoms::br);
  for (nsIContent* nextContent = getNextContent(aContent); nextContent;
       nextContent = getNextContent(*nextContent)) {
    if (nextContent->IsElement()) {
      // Break is right before a child block, it's not visible
      if (HTMLEditUtils::IsBlockElement(
              *nextContent, BlockInlineCheck::UseComputedDisplayStyle)) {
        return nextContent->AsElement();
      }

      // XXX How about other non-HTML elements?  Assume they are styled as
      //     blocks for now.
      if (!nextContent->IsHTMLElement()) {
        return nextContent->AsElement();
      }

      // If there is a visible content which generates something visible,
      // stop scanning.
      if (HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*nextContent)) {
        return nullptr;
      }

      if (nextContent->IsHTMLElement(nsGkAtoms::br)) {
        // If aContent is a <br> element, another <br> element prevents the
        // block boundary special handling.
        if (isBRElement) {
          return nullptr;
        }

        MOZ_ASSERT(aContent.IsText());
        // Following <br> element always hides its following block boundary.
        // I.e., white-spaces is at end of the text node is visible.
        if (aDirection == WalkTreeDirection::Forward) {
          return nullptr;
        }
        // Otherwise, if text node follows <br> element, its white-spaces at
        // start of the text node are invisible.  In this case, we return
        // the found <br> element.
        return nextContent->AsElement();
      }

      continue;
    }

    switch (nextContent->NodeType()) {
      case nsINode::TEXT_NODE:
      case nsINode::CDATA_SECTION_NODE:
        break;
      default:
        continue;
    }

    Text* textNode = Text::FromNode(nextContent);
    MOZ_ASSERT(textNode);
    if (!textNode->TextLength()) {
      continue;  // empty invisible text node, keep scanning next one.
    }
    if (HTMLEditUtils::IsInclusiveAncestorCSSDisplayNone(*textNode)) {
      continue;  // Styled as invisible.
    }
    if (!textNode->TextIsOnlyWhitespace()) {
      return nullptr;  // found a visible text node.
    }
    const nsTextFragment& textFragment = textNode->TextFragment();
    const bool isWhiteSpacePreformatted =
        EditorUtils::IsWhiteSpacePreformatted(*textNode);
    const bool isNewLinePreformatted =
        EditorUtils::IsNewLinePreformatted(*textNode);
    if (!isWhiteSpacePreformatted && !isNewLinePreformatted) {
      // if the white-space only text node is not preformatted, ignore it.
      continue;
    }
    for (uint32_t i = 0; i < textFragment.GetLength(); i++) {
      if (textFragment.CharAt(i) == HTMLEditUtils::kNewLine) {
        if (isNewLinePreformatted) {
          return nullptr;  // found a visible text node.
        }
        continue;
      }
      if (isWhiteSpacePreformatted) {
        return nullptr;  // found a visible text node.
      }
    }
    // All white-spaces in the text node is invisible, keep scanning next one.
  }

  // There is no visible content and reached current block boundary.  Then,
  // the <br> element is the last content in the block and invisible.
  // XXX Should we treat it visible if it's the only child of a block?
  return maybeNonEditableAncestorBlock;
}

nsIContent* HTMLEditUtils::GetUnnecessaryLineBreakContent(
    const Element& aBlockElement, ScanLineBreak aScanLineBreak) {
  auto* lastLineBreakContent = [&]() -> nsIContent* {
    const LeafNodeTypes leafNodeOrNonEditableNode{
        LeafNodeType::LeafNodeOrNonEditableNode};
    const WalkTreeOptions onlyPrecedingLine{
        WalkTreeOption::StopAtBlockBoundary};
    for (nsIContent* content =
             aScanLineBreak == ScanLineBreak::AtEndOfBlock
                 ? HTMLEditUtils::GetLastLeafContent(aBlockElement,
                                                     leafNodeOrNonEditableNode)
                 : HTMLEditUtils::GetPreviousContent(
                       aBlockElement, onlyPrecedingLine,
                       BlockInlineCheck::UseComputedDisplayStyle,
                       aBlockElement.GetParentElement());
         content;
         content =
             aScanLineBreak == ScanLineBreak::AtEndOfBlock
                 ? HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
                       *content, aBlockElement, leafNodeOrNonEditableNode,
                       BlockInlineCheck::UseComputedDisplayStyle)
                 : HTMLEditUtils::GetPreviousContent(
                       *content, onlyPrecedingLine,
                       BlockInlineCheck::UseComputedDisplayStyle,
                       aBlockElement.GetParentElement())) {
      // If we're scanning preceding <br> element of aBlockElement, we don't
      // need to look for a line break in another block because the caller
      // needs to handle only preceding <br> element of aBlockElement.
      if (aScanLineBreak == ScanLineBreak::BeforeBlock &&
          HTMLEditUtils::IsBlockElement(
              *content, BlockInlineCheck::UseComputedDisplayStyle)) {
        return nullptr;
      }
      if (Text* textNode = Text::FromNode(content)) {
        if (!textNode->TextLength()) {
          continue;  // ignore empty text node
        }
        const nsTextFragment& textFragment = textNode->TextFragment();
        if (EditorUtils::IsNewLinePreformatted(*textNode) &&
            textFragment.CharAt(textFragment.GetLength() - 1u) ==
                HTMLEditUtils::kNewLine) {
          // If the text node ends with a preserved line break, it's unnecessary
          // unless it follows another preformatted line break.
          if (textFragment.GetLength() == 1u) {
            return textNode;  // Need to scan previous leaf.
          }
          return textFragment.CharAt(textFragment.GetLength() - 2u) ==
                         HTMLEditUtils::kNewLine
                     ? nullptr
                     : textNode;
        }
        if (HTMLEditUtils::IsVisibleTextNode(*textNode)) {
          return nullptr;
        }
        continue;
      }
      if (content->IsCharacterData()) {
        continue;  // ignore hidden character data nodes like comment
      }
      if (content->IsHTMLElement(nsGkAtoms::br)) {
        return content;
      }
      // If found element is empty block or visible element, there is no
      // unnecessary line break.
      if (HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*content)) {
        return nullptr;
      }
      // Otherwise, e.g., empty <b>, we should keep scanning.
    }
    return nullptr;
  }();
  if (!lastLineBreakContent) {
    return nullptr;
  }

  // If the found node is a text node and contains only one preformatted new
  // line break, we need to keep scanning previous one, but if it has 2 or more
  // characters, we know it has redundant line break.
  Text* lastLineBreakText = Text::FromNode(lastLineBreakContent);
  if (lastLineBreakText && lastLineBreakText->TextDataLength() != 1u) {
    return lastLineBreakText;
  }

  // Scan previous leaf content, but now, we can stop at child block boundary.
  const LeafNodeTypes leafNodeOrNonEditableNodeOrChildBlock{
      LeafNodeType::LeafNodeOrNonEditableNode,
      LeafNodeType::LeafNodeOrChildBlock};
  const Element* blockElement = HTMLEditUtils::GetAncestorElement(
      *lastLineBreakContent, HTMLEditUtils::ClosestBlockElement,
      BlockInlineCheck::UseComputedDisplayStyle);
  for (nsIContent* content =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *lastLineBreakContent, *blockElement,
               leafNodeOrNonEditableNodeOrChildBlock,
               BlockInlineCheck::UseComputedDisplayStyle);
       content;
       content = HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
           *content, *blockElement, leafNodeOrNonEditableNodeOrChildBlock,
           BlockInlineCheck::UseComputedDisplayStyle)) {
    if (HTMLEditUtils::IsBlockElement(
            *content, BlockInlineCheck::UseComputedDisplayStyle) ||
        (content->IsElement() && !content->IsHTMLElement())) {
      // Now, must found <div>...<div>...</div><br></div>
      //                                       ^^^^
      // In this case, the <br> element is necessary to make a following empty
      // line of the inner <div> visible.
      return nullptr;
    }
    if (Text* textNode = Text::FromNode(content)) {
      if (!textNode->TextLength()) {
        continue;  // ignore empty text node
      }
      const nsTextFragment& textFragment = textNode->TextFragment();
      if (EditorUtils::IsNewLinePreformatted(*textNode) &&
          textFragment.CharAt(textFragment.GetLength() - 1u) ==
              HTMLEditUtils::kNewLine) {
        // So, we are here because the preformatted line break is followed by
        // lastLineBreakContent which is <br> or a text node containing only
        // one.  In this case, even if their parents are different,
        // lastLineBreakContent is necessary to make the last line visible.
        return nullptr;
      }
      if (!HTMLEditUtils::IsVisibleTextNode(*textNode)) {
        continue;
      }
      if (EditorUtils::IsWhiteSpacePreformatted(*textNode)) {
        // If the white-space is preserved, neither following <br> nor a
        // preformatted line break is not necessary.
        return lastLineBreakContent;
      }
      // Otherwise, only if the last character is a collapsible white-space,
      // we need lastLineBreakContent to make the trailing white-space visible.
      switch (textFragment.CharAt(textFragment.GetLength() - 1u)) {
        case HTMLEditUtils::kSpace:
        case HTMLEditUtils::kCarriageReturn:
        case HTMLEditUtils::kTab:
        case HTMLEditUtils::kNBSP:
          return nullptr;
        default:
          return lastLineBreakContent;
      }
    }
    if (content->IsCharacterData()) {
      continue;  // ignore hidden character data nodes like comment
    }
    // If lastLineBreakContent follows a <br> element in same block, it's
    // necessary to make the empty last line visible.
    if (content->IsHTMLElement(nsGkAtoms::br)) {
      return nullptr;
    }
    // If it follows a visible element, it's unnecessary line break.
    if (HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*content)) {
      return lastLineBreakContent;
    }
    // Otherwise, ignore empty inline elements such as <b>.
  }
  // If the block is empty except invisible data nodes and lastLineBreakContent,
  // lastLineBreakContent is necessary to make the block visible.
  return nullptr;
}

bool HTMLEditUtils::IsEmptyNode(nsPresContext* aPresContext,
                                const nsINode& aNode,
                                const EmptyCheckOptions& aOptions /* = {} */,
                                bool* aSeenBR /* = nullptr */) {
  MOZ_ASSERT_IF(aOptions.contains(EmptyCheckOption::SafeToAskLayout),
                aPresContext);

  if (aSeenBR) {
    *aSeenBR = false;
  }

  if (const Text* text = Text::FromNode(&aNode)) {
    return aOptions.contains(EmptyCheckOption::SafeToAskLayout)
               ? !IsInVisibleTextFrames(aPresContext, *text)
               : !IsVisibleTextNode(*text);
  }

  if (!aNode.IsElement()) {
    return false;
  }

  if (
      // If it's not a container such as an <hr> or <br>, etc, it should be
      // treated as not empty.
      !IsContainerNode(*aNode.AsContent()) ||
      // If it's a named anchor, we shouldn't treat it as empty because it
      // has special meaning even if invisible.
      IsNamedAnchor(&aNode) ||
      // Form widgets should be treated as not empty because they have special
      // meaning even if invisible.
      IsFormWidget(&aNode)) {
    return false;
  }

  const auto [isListItem, isTableCell, hasAppearance] =
      [&]() MOZ_NEVER_INLINE_DEBUG -> std::tuple<bool, bool, bool> {
    if (!StaticPrefs::editor_block_inline_check_use_computed_style()) {
      return {IsListItem(&aNode), IsTableCell(&aNode), false};
    }
    // Let's stop treating the document element and the <body> as a list item
    // nor a table cell to avoid tricky cases.
    if (aNode.OwnerDoc()->GetDocumentElement() == &aNode ||
        (aNode.IsHTMLElement(nsGkAtoms::body) &&
         aNode.OwnerDoc()->GetBodyElement() == &aNode)) {
      return {false, false, false};
    }

    RefPtr<const ComputedStyle> elementStyle =
        nsComputedDOMStyle::GetComputedStyleNoFlush(aNode.AsElement());
    // If there is no style information like in a document fragment, let's refer
    // the default style.
    if (MOZ_UNLIKELY(!elementStyle)) {
      return {IsListItem(&aNode), IsTableCell(&aNode), false};
    }
    const nsStyleDisplay* styleDisplay = elementStyle->StyleDisplay();
    if (NS_WARN_IF(!styleDisplay)) {
      return {IsListItem(&aNode), IsTableCell(&aNode), false};
    }
    if (styleDisplay->mDisplay != StyleDisplay::None &&
        styleDisplay->HasAppearance()) {
      return {false, false, true};
    }
    if (styleDisplay->IsListItem()) {
      return {true, false, false};
    }
    if (styleDisplay->mDisplay == StyleDisplay::TableCell) {
      return {false, true, false};
    }
    // The default display of <dt> and <dd> is block.  Therefore, we need
    // special handling for them.
    return {styleDisplay->mDisplay == StyleDisplay::Block &&
                aNode.IsAnyOfHTMLElements(nsGkAtoms::dd, nsGkAtoms::dt),
            false, false};
  }();

  // The web author created native widget without form control elements.  Let's
  // treat it as visible.
  if (hasAppearance) {
    return false;
  }

  if (isListItem &&
      aOptions.contains(EmptyCheckOption::TreatListItemAsVisible)) {
    return false;
  }
  if (isTableCell &&
      aOptions.contains(EmptyCheckOption::TreatTableCellAsVisible)) {
    return false;
  }

  bool seenBR = aSeenBR && *aSeenBR;
  for (nsIContent* childContent = aNode.GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    // Is the child editable and non-empty?  if so, return false
    if (aOptions.contains(
            EmptyCheckOption::TreatNonEditableContentAsInvisible) &&
        !EditorUtils::IsEditableContent(*childContent, EditorType::HTML)) {
      continue;
    }

    if (Text* text = Text::FromNode(childContent)) {
      // break out if we find we aren't empty
      if (aOptions.contains(EmptyCheckOption::SafeToAskLayout)
              ? IsInVisibleTextFrames(aPresContext, *text)
              : IsVisibleTextNode(*text)) {
        return false;
      }
      continue;
    }

    MOZ_ASSERT(childContent != &aNode);

    if (!aOptions.contains(EmptyCheckOption::TreatSingleBRElementAsVisible) &&
        !seenBR && childContent->IsHTMLElement(nsGkAtoms::br)) {
      // Ignore first <br> element in it if caller wants so because it's
      // typically a padding <br> element of for a parent block.
      seenBR = true;
      if (aSeenBR) {
        *aSeenBR = true;
      }
      continue;
    }

    // Note: list items or table cells are not considered empty
    // if they contain other lists or tables
    EmptyCheckOptions options(aOptions);
    if (childContent->IsElement() && (isListItem || isTableCell)) {
      options += {EmptyCheckOption::TreatListItemAsVisible,
                  EmptyCheckOption::TreatTableCellAsVisible};
    }
    if (!IsEmptyNode(aPresContext, *childContent, options, &seenBR)) {
      if (aSeenBR) {
        *aSeenBR = seenBR;
      }
      return false;
    }
  }

  if (aSeenBR) {
    *aSeenBR = seenBR;
  }
  return true;
}

bool HTMLEditUtils::ShouldInsertLinefeedCharacter(
    const EditorDOMPoint& aPointToInsert, const Element& aEditingHost) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  if (!aPointToInsert.IsInContentNode()) {
    return false;
  }

  // closestEditableBlockElement can be nullptr if aEditingHost is an inline
  // element.
  Element* closestEditableBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aPointToInsert.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestEditableBlockElement,
          BlockInlineCheck::UseComputedDisplayOutsideStyle);

  // If and only if the nearest block is the editing host or its parent,
  // and new line character is preformatted, we should insert a linefeed.
  return (!closestEditableBlockElement ||
          closestEditableBlockElement == &aEditingHost) &&
         EditorUtils::IsNewLinePreformatted(
             *aPointToInsert.ContainerAs<nsIContent>());
}

// We use bitmasks to test containment of elements. Elements are marked to be
// in certain groups by setting the mGroup member of the `ElementInfo` struct
// to the corresponding GROUP_ values (OR'ed together). Similarly, elements are
// marked to allow containment of certain groups by setting the
// mCanContainGroups member of the `ElementInfo` struct to the corresponding
// GROUP_ values (OR'ed together).
// Testing containment then simply consists of checking whether the
// mCanContainGroups bitmask of an element and the mGroup bitmask of a
// potential child overlap.

#define GROUP_NONE 0

// body, head, html
#define GROUP_TOPLEVEL (1 << 1)

// base, link, meta, script, style, title
#define GROUP_HEAD_CONTENT (1 << 2)

// b, big, i, s, small, strike, tt, u
#define GROUP_FONTSTYLE (1 << 3)

// abbr, acronym, cite, code, datalist, del, dfn, em, ins, kbd, mark, rb, rp
// rt, rtc, ruby, samp, strong, var
#define GROUP_PHRASE (1 << 4)

// a, applet, basefont, bdi, bdo, br, font, iframe, img, map, meter, object,
// output, picture, progress, q, script, span, sub, sup
#define GROUP_SPECIAL (1 << 5)

// button, form, input, label, select, textarea
#define GROUP_FORMCONTROL (1 << 6)

// address, applet, article, aside, blockquote, button, center, del, details,
// dialog, dir, div, dl, fieldset, figure, footer, form, h1, h2, h3, h4, h5,
// h6, header, hgroup, hr, iframe, ins, main, map, menu, nav, noframes,
// noscript, object, ol, p, pre, table, search, section, summary, ul
#define GROUP_BLOCK (1 << 7)

// frame, frameset
#define GROUP_FRAME (1 << 8)

// col, tbody
#define GROUP_TABLE_CONTENT (1 << 9)

// tr
#define GROUP_TBODY_CONTENT (1 << 10)

// td, th
#define GROUP_TR_CONTENT (1 << 11)

// col
#define GROUP_COLGROUP_CONTENT (1 << 12)

// param
#define GROUP_OBJECT_CONTENT (1 << 13)

// li
#define GROUP_LI (1 << 14)

// area
#define GROUP_MAP_CONTENT (1 << 15)

// optgroup, option
#define GROUP_SELECT_CONTENT (1 << 16)

// option
#define GROUP_OPTIONS (1 << 17)

// dd, dt
#define GROUP_DL_CONTENT (1 << 18)

// p
#define GROUP_P (1 << 19)

// text, white-space, newline, comment
#define GROUP_LEAF (1 << 20)

// XXX This is because the editor does sublists illegally.
// ol, ul
#define GROUP_OL_UL (1 << 21)

// h1, h2, h3, h4, h5, h6
#define GROUP_HEADING (1 << 22)

// figcaption
#define GROUP_FIGCAPTION (1 << 23)

// picture members (img, source)
#define GROUP_PICTURE_CONTENT (1 << 24)

#define GROUP_INLINE_ELEMENT                                            \
  (GROUP_FONTSTYLE | GROUP_PHRASE | GROUP_SPECIAL | GROUP_FORMCONTROL | \
   GROUP_LEAF)

#define GROUP_FLOW_ELEMENT (GROUP_INLINE_ELEMENT | GROUP_BLOCK)

struct ElementInfo final {
#ifdef DEBUG
  nsHTMLTag mTag;
#endif
  // See `GROUP_NONE`'s comment.
  uint32_t mGroup;
  // See `GROUP_NONE`'s comment.
  uint32_t mCanContainGroups;
  bool mIsContainer;
  bool mCanContainSelf;
};

#ifdef DEBUG
#  define ELEM(_tag, _isContainer, _canContainSelf, _group, _canContainGroups) \
    {                                                                          \
      eHTMLTag_##_tag, _group, _canContainGroups, _isContainer,                \
          _canContainSelf                                                      \
    }
#else
#  define ELEM(_tag, _isContainer, _canContainSelf, _group, _canContainGroups) \
    { _group, _canContainGroups, _isContainer, _canContainSelf }
#endif

static const ElementInfo kElements[eHTMLTag_userdefined] = {
    ELEM(a, true, false, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(abbr, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(acronym, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(address, true, true, GROUP_BLOCK, GROUP_INLINE_ELEMENT | GROUP_P),
    // While applet is no longer a valid tag, removing it here breaks the editor
    // (compiles, but causes many tests to fail in odd ways). This list is
    // tracked against the main HTML Tag list, so any changes will require more
    // than just removing entries.
    ELEM(applet, true, true, GROUP_SPECIAL | GROUP_BLOCK,
         GROUP_FLOW_ELEMENT | GROUP_OBJECT_CONTENT),
    ELEM(area, false, false, GROUP_MAP_CONTENT, GROUP_NONE),
    ELEM(article, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(aside, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(audio, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(b, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(base, false, false, GROUP_HEAD_CONTENT, GROUP_NONE),
    ELEM(basefont, false, false, GROUP_SPECIAL, GROUP_NONE),
    ELEM(bdi, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(bdo, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(bgsound, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(big, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(blockquote, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(body, true, true, GROUP_TOPLEVEL, GROUP_FLOW_ELEMENT),
    ELEM(br, false, false, GROUP_SPECIAL, GROUP_NONE),
    ELEM(button, true, true, GROUP_FORMCONTROL | GROUP_BLOCK,
         GROUP_FLOW_ELEMENT),
    ELEM(canvas, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(caption, true, true, GROUP_NONE, GROUP_INLINE_ELEMENT),
    ELEM(center, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(cite, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(code, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(col, false, false, GROUP_TABLE_CONTENT | GROUP_COLGROUP_CONTENT,
         GROUP_NONE),
    ELEM(colgroup, true, false, GROUP_NONE, GROUP_COLGROUP_CONTENT),
    ELEM(data, true, false, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(datalist, true, false, GROUP_PHRASE,
         GROUP_OPTIONS | GROUP_INLINE_ELEMENT),
    ELEM(dd, true, false, GROUP_DL_CONTENT, GROUP_FLOW_ELEMENT),
    ELEM(del, true, true, GROUP_PHRASE | GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(details, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(dfn, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(dialog, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(dir, true, false, GROUP_BLOCK, GROUP_LI),
    ELEM(div, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(dl, true, false, GROUP_BLOCK, GROUP_DL_CONTENT),
    ELEM(dt, true, true, GROUP_DL_CONTENT, GROUP_INLINE_ELEMENT),
    ELEM(em, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(embed, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(fieldset, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(figcaption, true, false, GROUP_FIGCAPTION, GROUP_FLOW_ELEMENT),
    ELEM(figure, true, true, GROUP_BLOCK,
         GROUP_FLOW_ELEMENT | GROUP_FIGCAPTION),
    ELEM(font, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(footer, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(form, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(frame, false, false, GROUP_FRAME, GROUP_NONE),
    ELEM(frameset, true, true, GROUP_FRAME, GROUP_FRAME),
    ELEM(h1, true, false, GROUP_BLOCK | GROUP_HEADING, GROUP_INLINE_ELEMENT),
    ELEM(h2, true, false, GROUP_BLOCK | GROUP_HEADING, GROUP_INLINE_ELEMENT),
    ELEM(h3, true, false, GROUP_BLOCK | GROUP_HEADING, GROUP_INLINE_ELEMENT),
    ELEM(h4, true, false, GROUP_BLOCK | GROUP_HEADING, GROUP_INLINE_ELEMENT),
    ELEM(h5, true, false, GROUP_BLOCK | GROUP_HEADING, GROUP_INLINE_ELEMENT),
    ELEM(h6, true, false, GROUP_BLOCK | GROUP_HEADING, GROUP_INLINE_ELEMENT),
    ELEM(head, true, false, GROUP_TOPLEVEL, GROUP_HEAD_CONTENT),
    ELEM(header, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(hgroup, true, false, GROUP_BLOCK, GROUP_HEADING),
    ELEM(hr, false, false, GROUP_BLOCK, GROUP_NONE),
    ELEM(html, true, false, GROUP_TOPLEVEL, GROUP_TOPLEVEL),
    ELEM(i, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(iframe, true, true, GROUP_SPECIAL | GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(image, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(img, false, false, GROUP_SPECIAL | GROUP_PICTURE_CONTENT, GROUP_NONE),
    ELEM(input, false, false, GROUP_FORMCONTROL, GROUP_NONE),
    ELEM(ins, true, true, GROUP_PHRASE | GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(kbd, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(keygen, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(label, true, false, GROUP_FORMCONTROL, GROUP_INLINE_ELEMENT),
    ELEM(legend, true, true, GROUP_NONE, GROUP_INLINE_ELEMENT),
    ELEM(li, true, false, GROUP_LI, GROUP_FLOW_ELEMENT),
    ELEM(link, false, false, GROUP_HEAD_CONTENT, GROUP_NONE),
    ELEM(listing, true, true, GROUP_BLOCK, GROUP_INLINE_ELEMENT),
    ELEM(main, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(map, true, true, GROUP_SPECIAL, GROUP_BLOCK | GROUP_MAP_CONTENT),
    ELEM(mark, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(marquee, true, false, GROUP_NONE, GROUP_NONE),
    ELEM(menu, true, true, GROUP_BLOCK, GROUP_LI | GROUP_FLOW_ELEMENT),
    ELEM(meta, false, false, GROUP_HEAD_CONTENT, GROUP_NONE),
    ELEM(meter, true, false, GROUP_SPECIAL, GROUP_FLOW_ELEMENT),
    ELEM(multicol, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(nav, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(nobr, true, false, GROUP_NONE, GROUP_NONE),
    ELEM(noembed, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(noframes, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(noscript, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(object, true, true, GROUP_SPECIAL | GROUP_BLOCK,
         GROUP_FLOW_ELEMENT | GROUP_OBJECT_CONTENT),
    // XXX Can contain self and ul because editor does sublists illegally.
    ELEM(ol, true, true, GROUP_BLOCK | GROUP_OL_UL, GROUP_LI | GROUP_OL_UL),
    ELEM(optgroup, true, false, GROUP_SELECT_CONTENT, GROUP_OPTIONS),
    ELEM(option, true, false, GROUP_SELECT_CONTENT | GROUP_OPTIONS, GROUP_LEAF),
    ELEM(output, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(p, true, false, GROUP_BLOCK | GROUP_P, GROUP_INLINE_ELEMENT),
    ELEM(param, false, false, GROUP_OBJECT_CONTENT, GROUP_NONE),
    ELEM(picture, true, false, GROUP_SPECIAL, GROUP_PICTURE_CONTENT),
    ELEM(plaintext, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(pre, true, true, GROUP_BLOCK, GROUP_INLINE_ELEMENT),
    ELEM(progress, true, false, GROUP_SPECIAL, GROUP_FLOW_ELEMENT),
    ELEM(q, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(rb, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(rp, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(rt, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(rtc, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(ruby, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(s, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(samp, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(script, true, false, GROUP_HEAD_CONTENT | GROUP_SPECIAL, GROUP_LEAF),
    ELEM(search, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(section, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(select, true, false, GROUP_FORMCONTROL, GROUP_SELECT_CONTENT),
    ELEM(small, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(slot, true, false, GROUP_NONE, GROUP_FLOW_ELEMENT),
    ELEM(source, false, false, GROUP_PICTURE_CONTENT, GROUP_NONE),
    ELEM(span, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(strike, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(strong, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(style, true, false, GROUP_HEAD_CONTENT, GROUP_LEAF),
    ELEM(sub, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(summary, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
    ELEM(sup, true, true, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
    ELEM(table, true, false, GROUP_BLOCK, GROUP_TABLE_CONTENT),
    ELEM(tbody, true, false, GROUP_TABLE_CONTENT, GROUP_TBODY_CONTENT),
    ELEM(td, true, false, GROUP_TR_CONTENT, GROUP_FLOW_ELEMENT),
    ELEM(textarea, true, false, GROUP_FORMCONTROL, GROUP_LEAF),
    ELEM(tfoot, true, false, GROUP_NONE, GROUP_TBODY_CONTENT),
    ELEM(th, true, false, GROUP_TR_CONTENT, GROUP_FLOW_ELEMENT),
    ELEM(thead, true, false, GROUP_NONE, GROUP_TBODY_CONTENT),
    ELEM(template, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(time, true, false, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(title, true, false, GROUP_HEAD_CONTENT, GROUP_LEAF),
    ELEM(tr, true, false, GROUP_TBODY_CONTENT, GROUP_TR_CONTENT),
    ELEM(track, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(tt, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    ELEM(u, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
    // XXX Can contain self and ol because editor does sublists illegally.
    ELEM(ul, true, true, GROUP_BLOCK | GROUP_OL_UL, GROUP_LI | GROUP_OL_UL),
    ELEM(var, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
    ELEM(video, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(wbr, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(xmp, true, false, GROUP_BLOCK, GROUP_NONE),

    // These aren't elements.
    ELEM(text, false, false, GROUP_LEAF, GROUP_NONE),
    ELEM(whitespace, false, false, GROUP_LEAF, GROUP_NONE),
    ELEM(newline, false, false, GROUP_LEAF, GROUP_NONE),
    ELEM(comment, false, false, GROUP_LEAF, GROUP_NONE),
    ELEM(entity, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(doctypeDecl, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(markupDecl, false, false, GROUP_NONE, GROUP_NONE),
    ELEM(instruction, false, false, GROUP_NONE, GROUP_NONE),

    ELEM(userdefined, true, false, GROUP_NONE, GROUP_FLOW_ELEMENT)};

bool HTMLEditUtils::CanNodeContain(nsHTMLTag aParentTagId,
                                   nsHTMLTag aChildTagId) {
  NS_ASSERTION(
      aParentTagId > eHTMLTag_unknown && aParentTagId <= eHTMLTag_userdefined,
      "aParentTagId out of range!");
  NS_ASSERTION(
      aChildTagId > eHTMLTag_unknown && aChildTagId <= eHTMLTag_userdefined,
      "aChildTagId out of range!");

#ifdef DEBUG
  static bool checked = false;
  if (!checked) {
    checked = true;
    int32_t i;
    for (i = 1; i <= eHTMLTag_userdefined; ++i) {
      NS_ASSERTION(kElements[i - 1].mTag == i,
                   "You need to update kElements (missing tags).");
    }
  }
#endif

  // Special-case button.
  if (aParentTagId == eHTMLTag_button) {
    static const nsHTMLTag kButtonExcludeKids[] = {
        eHTMLTag_a,     eHTMLTag_fieldset, eHTMLTag_form,    eHTMLTag_iframe,
        eHTMLTag_input, eHTMLTag_select,   eHTMLTag_textarea};

    uint32_t j;
    for (j = 0; j < ArrayLength(kButtonExcludeKids); ++j) {
      if (kButtonExcludeKids[j] == aChildTagId) {
        return false;
      }
    }
  }

  // Deprecated elements.
  if (aChildTagId == eHTMLTag_bgsound) {
    return false;
  }

  // Bug #67007, dont strip userdefined tags.
  if (aChildTagId == eHTMLTag_userdefined) {
    return true;
  }

  const ElementInfo& parent = kElements[aParentTagId - 1];
  if (aParentTagId == aChildTagId) {
    return parent.mCanContainSelf;
  }

  const ElementInfo& child = kElements[aChildTagId - 1];
  return !!(parent.mCanContainGroups & child.mGroup);
}

bool HTMLEditUtils::ContentIsInert(const nsIContent& aContent) {
  for (nsIContent* content :
       aContent.InclusiveFlatTreeAncestorsOfType<nsIContent>()) {
    if (nsIFrame* frame = content->GetPrimaryFrame()) {
      return frame->StyleUI()->IsInert();
    }
    // If it doesn't have primary frame, we need to check its ancestors.
    // This may occur if it's an invisible text node or element nodes whose
    // display is an invisible value.
    if (!content->IsElement()) {
      continue;
    }
    if (content->AsElement()->State().HasState(dom::ElementState::INERT)) {
      return true;
    }
  }
  return false;
}

bool HTMLEditUtils::IsContainerNode(nsHTMLTag aTagId) {
  NS_ASSERTION(aTagId > eHTMLTag_unknown && aTagId <= eHTMLTag_userdefined,
               "aTagId out of range!");

  return kElements[aTagId - 1].mIsContainer;
}

bool HTMLEditUtils::IsNonListSingleLineContainer(const nsINode& aNode) {
  return aNode.IsAnyOfHTMLElements(
      nsGkAtoms::address, nsGkAtoms::div, nsGkAtoms::h1, nsGkAtoms::h2,
      nsGkAtoms::h3, nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6,
      nsGkAtoms::listing, nsGkAtoms::p, nsGkAtoms::pre, nsGkAtoms::xmp);
}

bool HTMLEditUtils::IsSingleLineContainer(const nsINode& aNode) {
  return IsNonListSingleLineContainer(aNode) ||
         aNode.IsAnyOfHTMLElements(nsGkAtoms::li, nsGkAtoms::dt, nsGkAtoms::dd);
}

// static
template <typename PT, typename CT>
nsIContent* HTMLEditUtils::GetPreviousContent(
    const EditorDOMPointBase<PT, CT>& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  NS_WARNING_ASSERTION(
      !aPoint.IsInDataNode() || aPoint.IsInTextNode(),
      "GetPreviousContent() doesn't assume that the start point is a "
      "data node except text node");

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (aPoint.IsStartOfContainer() || aPoint.IsInTextNode()) {
    if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
        aPoint.IsInContentNode() &&
        HTMLEditUtils::IsBlockElement(
            *aPoint.template ContainerAs<nsIContent>(), aBlockInlineCheck)) {
      // If we aren't allowed to cross blocks, don't look before this block.
      return nullptr;
    }
    return HTMLEditUtils::GetPreviousContent(
        *aPoint.GetContainer(), aOptions, aBlockInlineCheck, aAncestorLimiter);
  }

  // else look before the child at 'aOffset'
  if (aPoint.GetChild()) {
    return HTMLEditUtils::GetPreviousContent(
        *aPoint.GetChild(), aOptions, aBlockInlineCheck, aAncestorLimiter);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the deep-right child.
  nsIContent* lastLeafContent = HTMLEditUtils::GetLastLeafContent(
      *aPoint.GetContainer(),
      {aOptions.contains(WalkTreeOption::StopAtBlockBoundary)
           ? LeafNodeType::LeafNodeOrChildBlock
           : LeafNodeType::OnlyLeafNode},
      aBlockInlineCheck);
  if (!lastLeafContent) {
    return nullptr;
  }

  if (!HTMLEditUtils::IsContentIgnored(*lastLeafContent, aOptions)) {
    return lastLeafContent;
  }

  // restart the search from the non-editable node we just found
  return HTMLEditUtils::GetPreviousContent(*lastLeafContent, aOptions,
                                           aBlockInlineCheck, aAncestorLimiter);
}

// static
template <typename PT, typename CT>
nsIContent* HTMLEditUtils::GetNextContent(
    const EditorDOMPointBase<PT, CT>& aPoint, const WalkTreeOptions& aOptions,
    BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  NS_WARNING_ASSERTION(
      !aPoint.IsInDataNode() || aPoint.IsInTextNode(),
      "GetNextContent() doesn't assume that the start point is a "
      "data node except text node");

  auto point = aPoint.template To<EditorRawDOMPoint>();

  // if the container is a text node, use its location instead
  if (point.IsInTextNode()) {
    point.SetAfter(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      return nullptr;
    }
  }

  if (point.GetChild()) {
    if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
        HTMLEditUtils::IsBlockElement(*point.GetChild(), aBlockInlineCheck)) {
      return point.GetChild();
    }

    nsIContent* firstLeafContent = HTMLEditUtils::GetFirstLeafContent(
        *point.GetChild(),
        {aOptions.contains(WalkTreeOption::StopAtBlockBoundary)
             ? LeafNodeType::LeafNodeOrChildBlock
             : LeafNodeType::OnlyLeafNode},
        aBlockInlineCheck);
    if (!firstLeafContent) {
      return point.GetChild();
    }

    // XXX Why do we need to do this check?  The leaf node must be a descendant
    //     of `point.GetChild()`.
    if (aAncestorLimiter &&
        (firstLeafContent == aAncestorLimiter ||
         !firstLeafContent->IsInclusiveDescendantOf(aAncestorLimiter))) {
      return nullptr;
    }

    if (!HTMLEditUtils::IsContentIgnored(*firstLeafContent, aOptions)) {
      return firstLeafContent;
    }

    // restart the search from the non-editable node we just found
    return HTMLEditUtils::GetNextContent(*firstLeafContent, aOptions,
                                         aBlockInlineCheck, aAncestorLimiter);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
      point.IsInContentNode() &&
      HTMLEditUtils::IsBlockElement(*point.template ContainerAs<nsIContent>(),
                                    aBlockInlineCheck)) {
    // don't cross out of parent block
    return nullptr;
  }

  return HTMLEditUtils::GetNextContent(*point.GetContainer(), aOptions,
                                       aBlockInlineCheck, aAncestorLimiter);
}

// static
nsIContent* HTMLEditUtils::GetAdjacentLeafContent(
    const nsINode& aNode, WalkTreeDirection aWalkTreeDirection,
    const WalkTreeOptions& aOptions, BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */) {
  // called only by GetPriorNode so we don't need to check params.
  MOZ_ASSERT(&aNode != aAncestorLimiter);
  MOZ_ASSERT_IF(aAncestorLimiter,
                aAncestorLimiter->IsInclusiveDescendantOf(aAncestorLimiter));

  const nsINode* node = &aNode;
  for (;;) {
    // if aNode has a sibling in the right direction, return
    // that sibling's closest child (or itself if it has no children)
    nsIContent* sibling = aWalkTreeDirection == WalkTreeDirection::Forward
                              ? node->GetNextSibling()
                              : node->GetPreviousSibling();
    if (sibling) {
      // XXX If `sibling` belongs to siblings of inclusive ancestors of aNode,
      //     perhaps, we need to use
      //     IgnoreInsideBlockBoundary(aBlockInlineCheck) here.
      if (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
          HTMLEditUtils::IsBlockElement(*sibling, aBlockInlineCheck)) {
        // don't look inside previous sibling, since it is a block
        return sibling;
      }
      const LeafNodeTypes leafNodeTypes = {
          aOptions.contains(WalkTreeOption::StopAtBlockBoundary)
              ? LeafNodeType::LeafNodeOrChildBlock
              : LeafNodeType::OnlyLeafNode};
      nsIContent* leafContent =
          aWalkTreeDirection == WalkTreeDirection::Forward
              ? HTMLEditUtils::GetFirstLeafContent(*sibling, leafNodeTypes,
                                                   aBlockInlineCheck)
              : HTMLEditUtils::GetLastLeafContent(*sibling, leafNodeTypes,
                                                  aBlockInlineCheck);
      return leafContent ? leafContent : sibling;
    }

    nsIContent* parent = node->GetParent();
    if (!parent) {
      return nullptr;
    }

    if (parent == aAncestorLimiter ||
        (aOptions.contains(WalkTreeOption::StopAtBlockBoundary) &&
         HTMLEditUtils::IsBlockElement(*parent, aBlockInlineCheck))) {
      return nullptr;
    }

    node = parent;
  }

  MOZ_ASSERT_UNREACHABLE("What part of for(;;) do you not understand?");
  return nullptr;
}

// static
nsIContent* HTMLEditUtils::GetAdjacentContent(
    const nsINode& aNode, WalkTreeDirection aWalkTreeDirection,
    const WalkTreeOptions& aOptions, BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */) {
  if (&aNode == aAncestorLimiter) {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.
    return nullptr;
  }

  nsIContent* leafContent = HTMLEditUtils::GetAdjacentLeafContent(
      aNode, aWalkTreeDirection, aOptions, aBlockInlineCheck, aAncestorLimiter);
  if (!leafContent) {
    return nullptr;
  }

  if (!HTMLEditUtils::IsContentIgnored(*leafContent, aOptions)) {
    return leafContent;
  }

  return HTMLEditUtils::GetAdjacentContent(*leafContent, aWalkTreeDirection,
                                           aOptions, aBlockInlineCheck,
                                           aAncestorLimiter);
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType HTMLEditUtils::GetPreviousEditablePoint(
    nsIContent& aContent, const Element* aAncestorLimiter,
    InvisibleWhiteSpaces aInvisibleWhiteSpaces,
    TableBoundary aHowToTreatTableBoundary) {
  MOZ_ASSERT(HTMLEditUtils::IsSimplyEditableNode(aContent));
  NS_ASSERTION(!HTMLEditUtils::IsAnyTableElement(&aContent) ||
                   HTMLEditUtils::IsTableCellOrCaption(aContent),
               "HTMLEditUtils::GetPreviousEditablePoint() may return a point "
               "between table structure elements");

  if (&aContent == aAncestorLimiter) {
    return EditorDOMPointType();
  }

  // First, look for previous content.
  nsIContent* previousContent = aContent.GetPreviousSibling();
  if (!previousContent) {
    if (!aContent.GetParentElement()) {
      return EditorDOMPointType();
    }
    nsIContent* inclusiveAncestor = &aContent;
    for (Element* parentElement : aContent.AncestorsOfType<Element>()) {
      if (parentElement == aAncestorLimiter ||
          !HTMLEditUtils::IsSimplyEditableNode(*parentElement) ||
          !HTMLEditUtils::CanCrossContentBoundary(*parentElement,
                                                  aHowToTreatTableBoundary)) {
        // If cannot cross the parent element boundary, return the point of
        // last inclusive ancestor point.
        return EditorDOMPointType(inclusiveAncestor);
      }

      // Start of the parent element is a next editable point if it's an
      // element which is not a table structure element.
      if (!HTMLEditUtils::IsAnyTableElement(parentElement) ||
          HTMLEditUtils::IsTableCellOrCaption(*parentElement)) {
        inclusiveAncestor = parentElement;
      }

      previousContent = parentElement->GetPreviousSibling();
      if (!previousContent) {
        continue;  // Keep looking for previous sibling of an ancestor.
      }

      // XXX Should we ignore data node like CDATA, Comment, etc?

      // If previous content is not editable, let's return the point after it.
      if (!HTMLEditUtils::IsSimplyEditableNode(*previousContent)) {
        return EditorDOMPointType::After(*previousContent);
      }

      // If cannot cross previous content boundary, return start of last
      // inclusive ancestor.
      if (!HTMLEditUtils::CanCrossContentBoundary(*previousContent,
                                                  aHowToTreatTableBoundary)) {
        return inclusiveAncestor == &aContent
                   ? EditorDOMPointType(inclusiveAncestor)
                   : EditorDOMPointType(inclusiveAncestor, 0);
      }
      break;
    }
    if (!previousContent) {
      return EditorDOMPointType(inclusiveAncestor);
    }
  } else if (!HTMLEditUtils::IsSimplyEditableNode(*previousContent)) {
    return EditorDOMPointType::After(*previousContent);
  } else if (!HTMLEditUtils::CanCrossContentBoundary(
                 *previousContent, aHowToTreatTableBoundary)) {
    return EditorDOMPointType(&aContent);
  }

  // Next, look for end of the previous content.
  nsIContent* leafContent = previousContent;
  if (previousContent->GetChildCount() &&
      HTMLEditUtils::IsContainerNode(*previousContent)) {
    for (nsIContent* maybeLeafContent = previousContent->GetLastChild();
         maybeLeafContent;
         maybeLeafContent = maybeLeafContent->GetLastChild()) {
      // If it's not an editable content or cannot cross the boundary,
      // return the point after the content.  Note that in this case,
      // the content must not be any table elements except `<table>`
      // because we've climbed down the tree.
      if (!HTMLEditUtils::IsSimplyEditableNode(*maybeLeafContent) ||
          !HTMLEditUtils::CanCrossContentBoundary(*maybeLeafContent,
                                                  aHowToTreatTableBoundary)) {
        return EditorDOMPointType::After(*maybeLeafContent);
      }
      leafContent = maybeLeafContent;
      if (!HTMLEditUtils::IsContainerNode(*leafContent)) {
        break;
      }
    }
  }

  if (leafContent->IsText()) {
    Text* textNode = leafContent->AsText();
    if (aInvisibleWhiteSpaces == InvisibleWhiteSpaces::Preserve) {
      return EditorDOMPointType::AtEndOf(*textNode);
    }
    // There may be invisible trailing white-spaces which should be
    // ignored.  Let's scan its start.
    return WSRunScanner::GetAfterLastVisiblePoint<EditorDOMPointType>(
        *textNode, aAncestorLimiter);
  }

  // If it's a container element, return end of it.  Otherwise, return
  // the point after the non-container element.
  return HTMLEditUtils::IsContainerNode(*leafContent)
             ? EditorDOMPointType::AtEndOf(*leafContent)
             : EditorDOMPointType::After(*leafContent);
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType HTMLEditUtils::GetNextEditablePoint(
    nsIContent& aContent, const Element* aAncestorLimiter,
    InvisibleWhiteSpaces aInvisibleWhiteSpaces,
    TableBoundary aHowToTreatTableBoundary) {
  MOZ_ASSERT(HTMLEditUtils::IsSimplyEditableNode(aContent));
  NS_ASSERTION(!HTMLEditUtils::IsAnyTableElement(&aContent) ||
                   HTMLEditUtils::IsTableCellOrCaption(aContent),
               "HTMLEditUtils::GetPreviousEditablePoint() may return a point "
               "between table structure elements");

  if (&aContent == aAncestorLimiter) {
    return EditorDOMPointType();
  }

  // First, look for next content.
  nsIContent* nextContent = aContent.GetNextSibling();
  if (!nextContent) {
    if (!aContent.GetParentElement()) {
      return EditorDOMPointType();
    }
    nsIContent* inclusiveAncestor = &aContent;
    for (Element* parentElement : aContent.AncestorsOfType<Element>()) {
      if (parentElement == aAncestorLimiter ||
          !HTMLEditUtils::IsSimplyEditableNode(*parentElement) ||
          !HTMLEditUtils::CanCrossContentBoundary(*parentElement,
                                                  aHowToTreatTableBoundary)) {
        // If cannot cross the parent element boundary, return the point of
        // last inclusive ancestor point.
        return EditorDOMPointType(inclusiveAncestor);
      }

      // End of the parent element is a next editable point if it's an
      // element which is not a table structure element.
      if (!HTMLEditUtils::IsAnyTableElement(parentElement) ||
          HTMLEditUtils::IsTableCellOrCaption(*parentElement)) {
        inclusiveAncestor = parentElement;
      }

      nextContent = parentElement->GetNextSibling();
      if (!nextContent) {
        continue;  // Keep looking for next sibling of an ancestor.
      }

      // XXX Should we ignore data node like CDATA, Comment, etc?

      // If next content is not editable, let's return the point after
      // the last inclusive ancestor.
      if (!HTMLEditUtils::IsSimplyEditableNode(*nextContent)) {
        return EditorDOMPointType::After(*parentElement);
      }

      // If cannot cross next content boundary, return after the last
      // inclusive ancestor.
      if (!HTMLEditUtils::CanCrossContentBoundary(*nextContent,
                                                  aHowToTreatTableBoundary)) {
        return EditorDOMPointType::After(*inclusiveAncestor);
      }
      break;
    }
    if (!nextContent) {
      return EditorDOMPointType::After(*inclusiveAncestor);
    }
  } else if (!HTMLEditUtils::IsSimplyEditableNode(*nextContent)) {
    return EditorDOMPointType::After(aContent);
  } else if (!HTMLEditUtils::CanCrossContentBoundary(
                 *nextContent, aHowToTreatTableBoundary)) {
    return EditorDOMPointType::After(aContent);
  }

  // Next, look for start of the next content.
  nsIContent* leafContent = nextContent;
  if (nextContent->GetChildCount() &&
      HTMLEditUtils::IsContainerNode(*nextContent)) {
    for (nsIContent* maybeLeafContent = nextContent->GetFirstChild();
         maybeLeafContent;
         maybeLeafContent = maybeLeafContent->GetFirstChild()) {
      // If it's not an editable content or cannot cross the boundary,
      // return the point at the content (i.e., start of its parent).  Note
      // that in this case, the content must not be any table elements except
      // `<table>` because we've climbed down the tree.
      if (!HTMLEditUtils::IsSimplyEditableNode(*maybeLeafContent) ||
          !HTMLEditUtils::CanCrossContentBoundary(*maybeLeafContent,
                                                  aHowToTreatTableBoundary)) {
        return EditorDOMPointType(maybeLeafContent);
      }
      leafContent = maybeLeafContent;
      if (!HTMLEditUtils::IsContainerNode(*leafContent)) {
        break;
      }
    }
  }

  if (leafContent->IsText()) {
    Text* textNode = leafContent->AsText();
    if (aInvisibleWhiteSpaces == InvisibleWhiteSpaces::Preserve) {
      return EditorDOMPointType(textNode, 0);
    }
    // There may be invisible leading white-spaces which should be
    // ignored.  Let's scan its start.
    return WSRunScanner::GetFirstVisiblePoint<EditorDOMPointType>(
        *textNode, aAncestorLimiter);
  }

  // If it's a container element, return start of it.  Otherwise, return
  // the point at the non-container element (i.e., start of its parent).
  return HTMLEditUtils::IsContainerNode(*leafContent)
             ? EditorDOMPointType(leafContent, 0)
             : EditorDOMPointType(leafContent);
}

// static
Element* HTMLEditUtils::GetAncestorElement(
    const nsIContent& aContent, const AncestorTypes& aAncestorTypes,
    BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */) {
  MOZ_ASSERT(
      aAncestorTypes.contains(AncestorType::ClosestBlockElement) ||
      aAncestorTypes.contains(AncestorType::MostDistantInlineElementInBlock) ||
      aAncestorTypes.contains(AncestorType::ButtonElement));

  if (&aContent == aAncestorLimiter) {
    return nullptr;
  }

  const Element* theBodyElement = aContent.OwnerDoc()->GetBody();
  const Element* theDocumentElement = aContent.OwnerDoc()->GetDocumentElement();
  Element* lastAncestorElement = nullptr;
  const bool editableElementOnly =
      aAncestorTypes.contains(AncestorType::EditableElement);
  const bool lookingForClosestBlockElement =
      aAncestorTypes.contains(AncestorType::ClosestBlockElement);
  const bool lookingForMostDistantInlineElementInBlock =
      aAncestorTypes.contains(AncestorType::MostDistantInlineElementInBlock);
  const bool ignoreHRElement =
      aAncestorTypes.contains(AncestorType::IgnoreHRElement);
  const bool lookingForButtonElement =
      aAncestorTypes.contains(AncestorType::ButtonElement);
  auto IsSearchingElementType = [&](const nsIContent& aContent) -> bool {
    if (!aContent.IsElement() ||
        (ignoreHRElement && aContent.IsHTMLElement(nsGkAtoms::hr))) {
      return false;
    }
    if (editableElementOnly &&
        !EditorUtils::IsEditableContent(aContent, EditorType::HTML)) {
      return false;
    }
    return (lookingForClosestBlockElement &&
            HTMLEditUtils::IsBlockElement(aContent, aBlockInlineCheck)) ||
           (lookingForMostDistantInlineElementInBlock &&
            HTMLEditUtils::IsInlineContent(aContent, aBlockInlineCheck)) ||
           (lookingForButtonElement &&
            aContent.IsHTMLElement(nsGkAtoms::button));
  };
  for (Element* element : aContent.AncestorsOfType<Element>()) {
    if (editableElementOnly &&
        !EditorUtils::IsEditableContent(*element, EditorType::HTML)) {
      return lastAncestorElement && IsSearchingElementType(*lastAncestorElement)
                 ? lastAncestorElement  // editing host (can be inline element)
                 : nullptr;
    }
    if (ignoreHRElement && element->IsHTMLElement(nsGkAtoms::hr)) {
      if (element == aAncestorLimiter) {
        break;
      }
      continue;
    }
    if (lookingForButtonElement && element->IsHTMLElement(nsGkAtoms::button)) {
      return element;  // closest button element
    }
    if (HTMLEditUtils::IsBlockElement(*element, aBlockInlineCheck)) {
      if (lookingForClosestBlockElement) {
        return element;  // closest block element
      }
      MOZ_ASSERT_IF(lastAncestorElement,
                    HTMLEditUtils::IsInlineContent(*lastAncestorElement,
                                                   aBlockInlineCheck));
      return lastAncestorElement;  // the last inline element which we found
    }
    if (element == aAncestorLimiter || element == theBodyElement ||
        element == theDocumentElement) {
      break;
    }
    lastAncestorElement = element;
  }
  return lastAncestorElement && IsSearchingElementType(*lastAncestorElement)
             ? lastAncestorElement
             : nullptr;
}

// static
Element* HTMLEditUtils::GetInclusiveAncestorElement(
    const nsIContent& aContent, const AncestorTypes& aAncestorTypes,
    BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */) {
  MOZ_ASSERT(
      aAncestorTypes.contains(AncestorType::ClosestBlockElement) ||
      aAncestorTypes.contains(AncestorType::MostDistantInlineElementInBlock) ||
      aAncestorTypes.contains(AncestorType::ButtonElement));

  const Element* theBodyElement = aContent.OwnerDoc()->GetBody();
  const Element* theDocumentElement = aContent.OwnerDoc()->GetDocumentElement();
  const bool editableElementOnly =
      aAncestorTypes.contains(AncestorType::EditableElement);
  const bool lookingForClosestBlockElement =
      aAncestorTypes.contains(AncestorType::ClosestBlockElement);
  const bool lookingForMostDistantInlineElementInBlock =
      aAncestorTypes.contains(AncestorType::MostDistantInlineElementInBlock);
  const bool lookingForButtonElement =
      aAncestorTypes.contains(AncestorType::ButtonElement);
  const bool ignoreHRElement =
      aAncestorTypes.contains(AncestorType::IgnoreHRElement);
  auto IsSearchingElementType = [&](const nsIContent& aContent) -> bool {
    if (!aContent.IsElement() ||
        (ignoreHRElement && aContent.IsHTMLElement(nsGkAtoms::hr))) {
      return false;
    }
    if (editableElementOnly &&
        !EditorUtils::IsEditableContent(aContent, EditorType::HTML)) {
      return false;
    }
    return (lookingForClosestBlockElement &&
            HTMLEditUtils::IsBlockElement(aContent, aBlockInlineCheck)) ||
           (lookingForMostDistantInlineElementInBlock &&
            HTMLEditUtils::IsInlineContent(aContent, aBlockInlineCheck)) ||
           (lookingForButtonElement &&
            aContent.IsHTMLElement(nsGkAtoms::button));
  };

  // If aContent is the body element or the document element, we shouldn't climb
  // up to its parent.
  if (editableElementOnly &&
      (&aContent == theBodyElement || &aContent == theDocumentElement)) {
    return IsSearchingElementType(aContent)
               ? const_cast<Element*>(aContent.AsElement())
               : nullptr;
  }

  if (lookingForButtonElement && aContent.IsHTMLElement(nsGkAtoms::button)) {
    return const_cast<Element*>(aContent.AsElement());
  }

  // If aContent is a block element, we don't need to climb up the tree.
  // Consider the result right now.
  if ((lookingForClosestBlockElement ||
       lookingForMostDistantInlineElementInBlock) &&
      HTMLEditUtils::IsBlockElement(aContent, aBlockInlineCheck) &&
      !(ignoreHRElement && aContent.IsHTMLElement(nsGkAtoms::hr))) {
    return IsSearchingElementType(aContent)
               ? const_cast<Element*>(aContent.AsElement())
               : nullptr;
  }

  // If aContent is the last element to search range because of the parent
  // element type, consider the result before calling GetAncestorElement()
  // because it won't return aContent.
  if (!aContent.GetParent() ||
      (editableElementOnly && !EditorUtils::IsEditableContent(
                                  *aContent.GetParent(), EditorType::HTML)) ||
      (!lookingForClosestBlockElement &&
       HTMLEditUtils::IsBlockElement(*aContent.GetParent(),
                                     aBlockInlineCheck) &&
       !(ignoreHRElement &&
         aContent.GetParent()->IsHTMLElement(nsGkAtoms::hr)))) {
    return IsSearchingElementType(aContent)
               ? const_cast<Element*>(aContent.AsElement())
               : nullptr;
  }

  if (&aContent == aAncestorLimiter) {
    return nullptr;
  }

  return HTMLEditUtils::GetAncestorElement(aContent, aAncestorTypes,
                                           aBlockInlineCheck, aAncestorLimiter);
}

// static
Element* HTMLEditUtils::GetClosestAncestorAnyListElement(
    const nsIContent& aContent) {
  for (Element* element : aContent.AncestorsOfType<Element>()) {
    if (HTMLEditUtils::IsAnyListElement(element)) {
      return element;
    }
  }
  return nullptr;
}

// static
Element* HTMLEditUtils::GetClosestInclusiveAncestorAnyListElement(
    const nsIContent& aContent) {
  for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
    if (HTMLEditUtils::IsAnyListElement(element)) {
      return element;
    }
  }
  return nullptr;
}

EditAction HTMLEditUtils::GetEditActionForInsert(const nsAtom& aTagName) {
  // This method may be in a hot path.  So, return only necessary
  // EditAction::eInsert*Element.
  if (&aTagName == nsGkAtoms::ul) {
    // For InputEvent.inputType, "insertUnorderedList".
    return EditAction::eInsertUnorderedListElement;
  }
  if (&aTagName == nsGkAtoms::ol) {
    // For InputEvent.inputType, "insertOrderedList".
    return EditAction::eInsertOrderedListElement;
  }
  if (&aTagName == nsGkAtoms::hr) {
    // For InputEvent.inputType, "insertHorizontalRule".
    return EditAction::eInsertHorizontalRuleElement;
  }
  return EditAction::eInsertNode;
}

EditAction HTMLEditUtils::GetEditActionForRemoveList(const nsAtom& aTagName) {
  // This method may be in a hot path.  So, return only necessary
  // EditAction::eRemove*Element.
  if (&aTagName == nsGkAtoms::ul) {
    // For InputEvent.inputType, "insertUnorderedList".
    return EditAction::eRemoveUnorderedListElement;
  }
  if (&aTagName == nsGkAtoms::ol) {
    // For InputEvent.inputType, "insertOrderedList".
    return EditAction::eRemoveOrderedListElement;
  }
  return EditAction::eRemoveListElement;
}

EditAction HTMLEditUtils::GetEditActionForInsert(const Element& aElement) {
  return GetEditActionForInsert(*aElement.NodeInfo()->NameAtom());
}

EditAction HTMLEditUtils::GetEditActionForFormatText(const nsAtom& aProperty,
                                                     const nsAtom* aAttribute,
                                                     bool aToSetStyle) {
  // This method may be in a hot path.  So, return only necessary
  // EditAction::eSet*Property or EditAction::eRemove*Property.
  if (&aProperty == nsGkAtoms::b) {
    return aToSetStyle ? EditAction::eSetFontWeightProperty
                       : EditAction::eRemoveFontWeightProperty;
  }
  if (&aProperty == nsGkAtoms::i) {
    return aToSetStyle ? EditAction::eSetTextStyleProperty
                       : EditAction::eRemoveTextStyleProperty;
  }
  if (&aProperty == nsGkAtoms::u) {
    return aToSetStyle ? EditAction::eSetTextDecorationPropertyUnderline
                       : EditAction::eRemoveTextDecorationPropertyUnderline;
  }
  if (&aProperty == nsGkAtoms::strike) {
    return aToSetStyle ? EditAction::eSetTextDecorationPropertyLineThrough
                       : EditAction::eRemoveTextDecorationPropertyLineThrough;
  }
  if (&aProperty == nsGkAtoms::sup) {
    return aToSetStyle ? EditAction::eSetVerticalAlignPropertySuper
                       : EditAction::eRemoveVerticalAlignPropertySuper;
  }
  if (&aProperty == nsGkAtoms::sub) {
    return aToSetStyle ? EditAction::eSetVerticalAlignPropertySub
                       : EditAction::eRemoveVerticalAlignPropertySub;
  }
  if (&aProperty == nsGkAtoms::font) {
    if (aAttribute == nsGkAtoms::face) {
      return aToSetStyle ? EditAction::eSetFontFamilyProperty
                         : EditAction::eRemoveFontFamilyProperty;
    }
    if (aAttribute == nsGkAtoms::color) {
      return aToSetStyle ? EditAction::eSetColorProperty
                         : EditAction::eRemoveColorProperty;
    }
    if (aAttribute == nsGkAtoms::bgcolor) {
      return aToSetStyle ? EditAction::eSetBackgroundColorPropertyInline
                         : EditAction::eRemoveBackgroundColorPropertyInline;
    }
  }
  return aToSetStyle ? EditAction::eSetInlineStyleProperty
                     : EditAction::eRemoveInlineStyleProperty;
}

EditAction HTMLEditUtils::GetEditActionForAlignment(
    const nsAString& aAlignType) {
  // This method may be in a hot path.  So, return only necessary
  // EditAction::eAlign*.
  if (aAlignType.EqualsLiteral("left")) {
    return EditAction::eAlignLeft;
  }
  if (aAlignType.EqualsLiteral("right")) {
    return EditAction::eAlignRight;
  }
  if (aAlignType.EqualsLiteral("center")) {
    return EditAction::eAlignCenter;
  }
  if (aAlignType.EqualsLiteral("justify")) {
    return EditAction::eJustify;
  }
  return EditAction::eSetAlignment;
}

// static
template <typename EditorDOMPointType>
nsIContent* HTMLEditUtils::GetContentToPreserveInlineStyles(
    const EditorDOMPointType& aPoint, const Element& aEditingHost) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  if (MOZ_UNLIKELY(!aPoint.IsInContentNode())) {
    return nullptr;
  }
  // If it points middle of a text node, use it.  Otherwise, scan next visible
  // thing and use the style of following text node if there is.
  if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer()) {
    return aPoint.template ContainerAs<nsIContent>();
  }
  for (auto point = aPoint.template To<EditorRawDOMPoint>(); point.IsSet();) {
    WSScanResult nextVisibleThing =
        WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(
            &aEditingHost, point,
            BlockInlineCheck::UseComputedDisplayOutsideStyle);
    if (nextVisibleThing.InVisibleOrCollapsibleCharacters()) {
      return nextVisibleThing.TextPtr();
    }
    // Ignore empty inline container elements because it's not visible for
    // users so that using the style will appear suddenly from point of
    // view of users.
    if (nextVisibleThing.ReachedSpecialContent() &&
        nextVisibleThing.IsContentEditable() &&
        nextVisibleThing.GetContent()->IsElement() &&
        !nextVisibleThing.GetContent()->HasChildNodes() &&
        HTMLEditUtils::IsContainerNode(*nextVisibleThing.ElementPtr())) {
      point.SetAfter(nextVisibleThing.ElementPtr());
      continue;
    }
    // Otherwise, we should use style of the container of the start point.
    break;
  }
  return aPoint.template ContainerAs<nsIContent>();
}

template <typename EditorDOMPointType, typename EditorDOMPointTypeInput>
EditorDOMPointType HTMLEditUtils::GetBetterInsertionPointFor(
    const nsIContent& aContentToInsert,
    const EditorDOMPointTypeInput& aPointToInsert,
    const Element& aEditingHost) {
  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return EditorDOMPointType();
  }

  auto pointToInsert =
      aPointToInsert.template GetNonAnonymousSubtreePoint<EditorDOMPointType>();
  if (MOZ_UNLIKELY(
          NS_WARN_IF(!pointToInsert.IsSet()) ||
          NS_WARN_IF(!pointToInsert.GetContainer()->IsInclusiveDescendantOf(
              &aEditingHost)))) {
    // Cannot insert aContentToInsert into this DOM tree.
    return EditorDOMPointType();
  }

  // If the node to insert is not a block level element, we can insert it
  // at any point.
  if (!HTMLEditUtils::IsBlockElement(
          aContentToInsert, BlockInlineCheck::UseComputedDisplayStyle)) {
    return pointToInsert;
  }

  WSRunScanner wsScannerForPointToInsert(
      const_cast<Element*>(&aEditingHost), pointToInsert,
      BlockInlineCheck::UseComputedDisplayStyle);

  // If the insertion position is after the last visible item in a line,
  // i.e., the insertion position is just before a visible line break <br>,
  // we want to skip to the position just after the line break (see bug 68767).
  WSScanResult forwardScanFromPointToInsertResult =
      wsScannerForPointToInsert.ScanNextVisibleNodeOrBlockBoundaryFrom(
          pointToInsert);
  // So, if the next visible node isn't a <br> element, we can insert the block
  // level element to the point.
  if (!forwardScanFromPointToInsertResult.GetContent() ||
      !forwardScanFromPointToInsertResult.ReachedBRElement()) {
    return pointToInsert;
  }

  // However, we must not skip next <br> element when the caret appears to be
  // positioned at the beginning of a block, in that case skipping the <br>
  // would not insert the <br> at the caret position, but after the current
  // empty line.
  WSScanResult backwardScanFromPointToInsertResult =
      wsScannerForPointToInsert.ScanPreviousVisibleNodeOrBlockBoundaryFrom(
          pointToInsert);
  // So, if there is no previous visible node,
  // or, if both nodes of the insertion point is <br> elements,
  // or, if the previous visible node is different block,
  // we need to skip the following <br>.  So, otherwise, we can insert the
  // block at the insertion point.
  if (!backwardScanFromPointToInsertResult.GetContent() ||
      backwardScanFromPointToInsertResult.ReachedBRElement() ||
      backwardScanFromPointToInsertResult.ReachedCurrentBlockBoundary()) {
    return pointToInsert;
  }

  return forwardScanFromPointToInsertResult
      .template PointAfterContent<EditorDOMPointType>();
}

// static
template <typename EditorDOMPointType, typename EditorDOMPointTypeInput>
EditorDOMPointType HTMLEditUtils::GetBetterCaretPositionToInsertText(
    const EditorDOMPointTypeInput& aPoint, const Element& aEditingHost) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT(
      aPoint.GetContainer()->IsInclusiveFlatTreeDescendantOf(&aEditingHost));

  if (aPoint.IsInTextNode()) {
    return aPoint.template To<EditorDOMPointType>();
  }
  if (!aPoint.IsEndOfContainer() && aPoint.GetChild() &&
      aPoint.GetChild()->IsText()) {
    return EditorDOMPointType(aPoint.GetChild(), 0u);
  }
  if (aPoint.IsEndOfContainer()) {
    WSRunScanner scanner(&aEditingHost, aPoint,
                         BlockInlineCheck::UseComputedDisplayStyle);
    WSScanResult previousThing =
        scanner.ScanPreviousVisibleNodeOrBlockBoundaryFrom(aPoint);
    if (previousThing.InVisibleOrCollapsibleCharacters()) {
      return EditorDOMPointType::AtEndOf(*previousThing.TextPtr());
    }
  }
  if (HTMLEditUtils::CanNodeContain(*aPoint.GetContainer(),
                                    *nsGkAtoms::textTagName)) {
    return aPoint.template To<EditorDOMPointType>();
  }
  if (MOZ_UNLIKELY(aPoint.GetContainer() == &aEditingHost ||
                   !aPoint.template GetContainerParentAs<nsIContent>() ||
                   !HTMLEditUtils::CanNodeContain(
                       *aPoint.template ContainerParentAs<nsIContent>(),
                       *nsGkAtoms::textTagName))) {
    return EditorDOMPointType();
  }
  return aPoint.ParentPoint().template To<EditorDOMPointType>();
}

// static
template <typename EditorDOMPointType, typename EditorDOMPointTypeInput>
Result<EditorDOMPointType, nsresult>
HTMLEditUtils::ComputePointToPutCaretInElementIfOutside(
    const Element& aElement, const EditorDOMPointTypeInput& aCurrentPoint) {
  MOZ_ASSERT(aCurrentPoint.IsSet());

  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/d3c2f51d89c3ca008ff0cb5a057e77ccd973443e/editor/libeditor/HTMLEditSubActionHandler.cpp#9193

  // Use range boundaries and RangeUtils::CompareNodeToRange() to compare
  // selection start to new block.
  bool nodeBefore, nodeAfter;
  nsresult rv = RangeUtils::CompareNodeToRangeBoundaries(
      const_cast<Element*>(&aElement), aCurrentPoint.ToRawRangeBoundary(),
      aCurrentPoint.ToRawRangeBoundary(), &nodeBefore, &nodeAfter);
  if (NS_FAILED(rv)) {
    NS_WARNING("RangeUtils::CompareNodeToRange() failed");
    return Err(rv);
  }

  if (nodeBefore && nodeAfter) {
    return EditorDOMPointType();  // aCurrentPoint is in aElement
  }

  if (nodeBefore) {
    // selection is after block.  put at end of block.
    const nsIContent* lastEditableContent = HTMLEditUtils::GetLastChild(
        aElement, {WalkTreeOption::IgnoreNonEditableNode});
    if (!lastEditableContent) {
      lastEditableContent = &aElement;
    }
    if (lastEditableContent->IsText() ||
        HTMLEditUtils::IsContainerNode(*lastEditableContent)) {
      return EditorDOMPointType::AtEndOf(*lastEditableContent);
    }
    MOZ_ASSERT(lastEditableContent->GetParentNode());
    return EditorDOMPointType::After(*lastEditableContent);
  }

  // selection is before block.  put at start of block.
  const nsIContent* firstEditableContent = HTMLEditUtils::GetFirstChild(
      aElement, {WalkTreeOption::IgnoreNonEditableNode});
  if (!firstEditableContent) {
    firstEditableContent = &aElement;
  }
  if (firstEditableContent->IsText() ||
      HTMLEditUtils::IsContainerNode(*firstEditableContent)) {
    MOZ_ASSERT(firstEditableContent->GetParentNode());
    // XXX Shouldn't this be EditorDOMPointType(firstEditableContent, 0u)?
    return EditorDOMPointType(firstEditableContent);
  }
  // XXX And shouldn't this be EditorDOMPointType(firstEditableContent)?
  return EditorDOMPointType(firstEditableContent, 0u);
}

// static
bool HTMLEditUtils::IsInlineStyleSetByElement(
    const nsIContent& aContent, const EditorInlineStyle& aStyle,
    const nsAString* aValue, nsAString* aOutValue /* = nullptr */) {
  for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
    if (aStyle.mHTMLProperty != element->NodeInfo()->NameAtom()) {
      continue;
    }
    if (!aStyle.mAttribute) {
      return true;
    }
    nsAutoString value;
    element->GetAttr(aStyle.mAttribute, value);
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

// static
size_t HTMLEditUtils::CollectChildren(
    const nsINode& aNode,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
    size_t aIndexToInsertChildren, const CollectChildrenOptions& aOptions) {
  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/4bce7d85ba4796dd03c5dcc7cfe8eee0e4c07b3b/editor/libeditor/HTMLEditSubActionHandler.cpp#6261

  size_t numberOfFoundChildren = 0;
  for (nsIContent* content =
           GetFirstChild(aNode, {WalkTreeOption::IgnoreNonEditableNode});
       content; content = content->GetNextSibling()) {
    if ((aOptions.contains(CollectChildrenOption::CollectListChildren) &&
         (HTMLEditUtils::IsAnyListElement(content) ||
          HTMLEditUtils::IsListItem(content))) ||
        (aOptions.contains(CollectChildrenOption::CollectTableChildren) &&
         HTMLEditUtils::IsAnyTableElement(content))) {
      numberOfFoundChildren += HTMLEditUtils::CollectChildren(
          *content, aOutArrayOfContents,
          aIndexToInsertChildren + numberOfFoundChildren, aOptions);
      continue;
    }

    if (aOptions.contains(CollectChildrenOption::IgnoreNonEditableChildren) &&
        !EditorUtils::IsEditableContent(*content, EditorType::HTML)) {
      continue;
    }
    if (aOptions.contains(CollectChildrenOption::IgnoreInvisibleTextNodes) &&
        content->IsText() &&
        !HTMLEditUtils::IsVisibleTextNode(*content->AsText())) {
      continue;
    }
    aOutArrayOfContents.InsertElementAt(
        aIndexToInsertChildren + numberOfFoundChildren++, *content);
  }
  return numberOfFoundChildren;
}

// static
size_t HTMLEditUtils::CollectEmptyInlineContainerDescendants(
    const nsINode& aNode,
    nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents,
    const EmptyCheckOptions& aOptions, BlockInlineCheck aBlockInlineCheck) {
  size_t numberOfFoundElements = 0;
  for (Element* element = aNode.GetFirstElementChild(); element;) {
    if (HTMLEditUtils::IsEmptyInlineContainer(*element, aOptions,
                                              aBlockInlineCheck)) {
      aOutArrayOfContents.AppendElement(*element);
      numberOfFoundElements++;
      nsIContent* nextContent = element->GetNextNonChildNode(&aNode);
      element = nullptr;
      for (; nextContent; nextContent = nextContent->GetNextNode(&aNode)) {
        if (nextContent->IsElement()) {
          element = nextContent->AsElement();
          break;
        }
      }
      continue;
    }

    nsIContent* nextContent = element->GetNextNode(&aNode);
    element = nullptr;
    for (; nextContent; nextContent = nextContent->GetNextNode(&aNode)) {
      if (nextContent->IsElement()) {
        element = nextContent->AsElement();
        break;
      }
    }
  }
  return numberOfFoundElements;
}

// static
bool HTMLEditUtils::ElementHasAttributeExcept(const Element& aElement,
                                              const nsAtom& aAttribute1,
                                              const nsAtom& aAttribute2,
                                              const nsAtom& aAttribute3) {
  // FYI: This was moved from
  // https://searchfox.org/mozilla-central/rev/0b1543e85d13c30a13c57e959ce9815a3f0fa1d3/editor/libeditor/HTMLStyleEditor.cpp#1626
  for (auto i : IntegerRange<uint32_t>(aElement.GetAttrCount())) {
    const nsAttrName* name = aElement.GetAttrNameAt(i);
    if (!name->NamespaceEquals(kNameSpaceID_None)) {
      return true;
    }

    if (name->LocalName() == &aAttribute1 ||
        name->LocalName() == &aAttribute2 ||
        name->LocalName() == &aAttribute3) {
      continue;  // Ignore the given attribute
    }

    // Ignore empty style, class and id attributes because those attributes are
    // not meaningful with empty value.
    if (name->LocalName() == nsGkAtoms::style ||
        name->LocalName() == nsGkAtoms::_class ||
        name->LocalName() == nsGkAtoms::id) {
      if (aElement.HasNonEmptyAttr(name->LocalName())) {
        return true;
      }
      continue;
    }

    // Ignore special _moz attributes
    nsAutoString attrString;
    name->LocalName()->ToString(attrString);
    if (!StringBeginsWith(attrString, u"_moz"_ns)) {
      return true;
    }
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return true
  return false;
}

bool HTMLEditUtils::GetNormalizedHTMLColorValue(const nsAString& aColorValue,
                                                nsAString& aNormalizedValue) {
  nsAttrValue value;
  if (!value.ParseColor(aColorValue)) {
    aNormalizedValue = aColorValue;
    return false;
  }
  nscolor color = NS_RGB(0, 0, 0);
  MOZ_ALWAYS_TRUE(value.GetColorValue(color));
  aNormalizedValue = NS_ConvertASCIItoUTF16(nsPrintfCString(
      "#%02x%02x%02x", NS_GET_R(color), NS_GET_G(color), NS_GET_B(color)));
  return true;
}

bool HTMLEditUtils::IsSameHTMLColorValue(
    const nsAString& aColorA, const nsAString& aColorB,
    TransparentKeyword aTransparentKeyword) {
  if (aTransparentKeyword == TransparentKeyword::Allowed) {
    const bool isATransparent = aColorA.LowerCaseEqualsLiteral("transparent");
    const bool isBTransparent = aColorB.LowerCaseEqualsLiteral("transparent");
    if (isATransparent || isBTransparent) {
      return isATransparent && isBTransparent;
    }
  }
  nsAttrValue valueA, valueB;
  if (!valueA.ParseColor(aColorA) || !valueB.ParseColor(aColorB)) {
    return false;
  }
  nscolor colorA = NS_RGB(0, 0, 0), colorB = NS_RGB(0, 0, 0);
  MOZ_ALWAYS_TRUE(valueA.GetColorValue(colorA));
  MOZ_ALWAYS_TRUE(valueB.GetColorValue(colorB));
  return colorA == colorB;
}

bool HTMLEditUtils::MaybeCSSSpecificColorValue(const nsAString& aColorValue) {
  if (aColorValue.IsEmpty() || aColorValue.First() == '#') {
    return false;  // Quick return for the most cases.
  }

  nsAutoString colorValue(aColorValue);
  colorValue.CompressWhitespace(true, true);
  if (colorValue.LowerCaseEqualsASCII("transparent")) {
    return true;
  }
  nscolor color = NS_RGB(0, 0, 0);
  if (colorValue.IsEmpty() || colorValue.First() == '#') {
    return false;
  }
  const NS_ConvertUTF16toUTF8 colorU8(colorValue);
  if (Servo_ColorNameToRgb(&colorU8, &color)) {
    return false;
  }
  if (colorValue.LowerCaseEqualsASCII("initial") ||
      colorValue.LowerCaseEqualsASCII("inherit") ||
      colorValue.LowerCaseEqualsASCII("unset") ||
      colorValue.LowerCaseEqualsASCII("revert") ||
      colorValue.LowerCaseEqualsASCII("currentcolor")) {
    return true;
  }
  return ServoCSSParser::IsValidCSSColor(colorU8);
}

static bool ComputeColor(const nsAString& aColorValue, nscolor* aColor,
                         bool* aIsCurrentColor) {
  return ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0),
                                      NS_ConvertUTF16toUTF8(aColorValue),
                                      aColor, aIsCurrentColor);
}

static bool ComputeColor(const nsACString& aColorValue, nscolor* aColor,
                         bool* aIsCurrentColor) {
  return ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), aColorValue,
                                      aColor, aIsCurrentColor);
}

bool HTMLEditUtils::CanConvertToHTMLColorValue(const nsAString& aColorValue) {
  bool isCurrentColor = false;
  nscolor color = NS_RGB(0, 0, 0);
  return ComputeColor(aColorValue, &color, &isCurrentColor) &&
         !isCurrentColor && NS_GET_A(color) == 0xFF;
}

bool HTMLEditUtils::ConvertToNormalizedHTMLColorValue(
    const nsAString& aColorValue, nsAString& aNormalizedValue) {
  bool isCurrentColor = false;
  nscolor color = NS_RGB(0, 0, 0);
  if (!ComputeColor(aColorValue, &color, &isCurrentColor) || isCurrentColor ||
      NS_GET_A(color) != 0xFF) {
    aNormalizedValue = aColorValue;
    return false;
  }
  aNormalizedValue.Truncate();
  aNormalizedValue.AppendPrintf("#%02x%02x%02x", NS_GET_R(color),
                                NS_GET_G(color), NS_GET_B(color));
  return true;
}

bool HTMLEditUtils::GetNormalizedCSSColorValue(const nsAString& aColorValue,
                                               ZeroAlphaColor aZeroAlphaColor,
                                               nsAString& aNormalizedValue) {
  bool isCurrentColor = false;
  nscolor color = NS_RGB(0, 0, 0);
  if (!ComputeColor(aColorValue, &color, &isCurrentColor)) {
    aNormalizedValue = aColorValue;
    return false;
  }

  // If it's currentcolor, let's return it as-is since we cannot resolve it
  // without ancestors.
  if (isCurrentColor) {
    aNormalizedValue = aColorValue;
    return true;
  }

  if (aZeroAlphaColor == ZeroAlphaColor::TransparentKeyword &&
      NS_GET_A(color) == 0) {
    aNormalizedValue.AssignLiteral("transparent");
    return true;
  }

  // Get serialized color value (i.e., "rgb()" or "rgba()").
  aNormalizedValue.Truncate();
  nsStyleUtil::GetSerializedColorValue(color, aNormalizedValue);
  return true;
}

template <typename CharType>
bool HTMLEditUtils::IsSameCSSColorValue(const nsTSubstring<CharType>& aColorA,
                                        const nsTSubstring<CharType>& aColorB) {
  bool isACurrentColor = false;
  nscolor colorA = NS_RGB(0, 0, 0);
  if (!ComputeColor(aColorA, &colorA, &isACurrentColor)) {
    return false;
  }
  bool isBCurrentColor = false;
  nscolor colorB = NS_RGB(0, 0, 0);
  if (!ComputeColor(aColorB, &colorB, &isBCurrentColor)) {
    return false;
  }
  if (isACurrentColor || isBCurrentColor) {
    return isACurrentColor && isBCurrentColor;
  }
  return colorA == colorB;
}

/******************************************************************************
 * SelectedTableCellScanner
 ******************************************************************************/

SelectedTableCellScanner::SelectedTableCellScanner(
    const AutoRangeArray& aRanges) {
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

}  // namespace mozilla
