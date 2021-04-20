/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditUtils.h"

#include "CSSEditUtils.h"  // for CSSEditUtils
#include "WSRunObject.h"   // for WSRunScanner

#include "mozilla/ArrayUtils.h"      // for ArrayLength
#include "mozilla/Assertions.h"      // for MOZ_ASSERT, etc.
#include "mozilla/EditAction.h"      // for EditAction
#include "mozilla/EditorBase.h"      // for EditorBase, EditorType
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint, etc.
#include "mozilla/EditorUtils.h"     // for EditorUtils
#include "mozilla/dom/Element.h"     // for Element, nsINode
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/Text.h"  // for Text

#include "nsAString.h"  // for nsAString::IsEmpty
#include "nsAtom.h"     // for nsAtom
#include "nsCaseTreatment.h"
#include "nsCOMPtr.h"        // for nsCOMPtr, operator==, etc.
#include "nsDebug.h"         // for NS_ASSERTION, etc.
#include "nsElementTable.h"  // for nsHTMLElement
#include "nsError.h"         // for NS_SUCCEEDED
#include "nsGkAtoms.h"       // for nsGkAtoms, nsGkAtoms::a, etc.
#include "nsHTMLTags.h"
#include "nsLiteralString.h"     // for NS_LITERAL_STRING
#include "nsNameSpaceManager.h"  // for kNameSpaceID_None
#include "nsString.h"            // for nsAutoString
#include "nsStyledElement.h"
#include "nsTextFragment.h"  // for nsTextFragment

namespace mozilla {

using namespace dom;
using EditorType = EditorBase::EditorType;

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

bool HTMLEditUtils::CanContentsBeJoined(const nsIContent& aLeftContent,
                                        const nsIContent& aRightContent,
                                        StyleDifference aStyleDifference) {
  if (aLeftContent.NodeInfo()->NameAtom() !=
      aRightContent.NodeInfo()->NameAtom()) {
    return false;
  }
  if (aStyleDifference == StyleDifference::Ignore ||
      !aLeftContent.IsElement()) {
    return true;
  }
  if (aStyleDifference == StyleDifference::CompareIfSpanElements &&
      !aLeftContent.IsHTMLElement(nsGkAtoms::span)) {
    return true;
  }
  if (!aLeftContent.IsElement() || !aRightContent.IsElement()) {
    return false;
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

bool HTMLEditUtils::IsBlockElement(const nsIContent& aContent) {
  if (!aContent.IsElement()) {
    return false;
  }
  if (aContent.IsHTMLElement(nsGkAtoms::br)) {  // shortcut for TextEditor
    MOZ_ASSERT(!nsHTMLElement::IsBlock(nsHTMLTags::AtomTagToId(nsGkAtoms::br)));
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
      nsHTMLTags::AtomTagToId(aContent.NodeInfo()->NameAtom()));
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
 * IsFormatNode() returns true if aNode is a format node.
 */
bool HTMLEditUtils::IsFormatNode(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(
      nsGkAtoms::p, nsGkAtoms::pre, nsGkAtoms::h1, nsGkAtoms::h2, nsGkAtoms::h3,
      nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6, nsGkAtoms::address);
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
bool HTMLEditUtils::IsListItem(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::li, nsGkAtoms::dd,
                                    nsGkAtoms::dt);
}

/**
 * IsAnyTableElement() returns true if aNode is an html table, td, tr, ...
 */
bool HTMLEditUtils::IsAnyTableElement(nsINode* aNode) {
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
bool HTMLEditUtils::IsTableCell(nsINode* aNode) {
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
bool HTMLEditUtils::IsAnyListElement(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::ul, nsGkAtoms::ol,
                                    nsGkAtoms::dl);
}

/**
 * IsPre() returns true if aNode is an html pre node.
 */
bool HTMLEditUtils::IsPre(nsINode* aNode) {
  return aNode && aNode->IsHTMLElement(nsGkAtoms::pre);
}

/**
 * IsImage() returns true if aNode is an html image node.
 */
bool HTMLEditUtils::IsImage(nsINode* aNode) {
  return aNode && aNode->IsHTMLElement(nsGkAtoms::img);
}

bool HTMLEditUtils::IsLink(nsINode* aNode) {
  MOZ_ASSERT(aNode);

  if (!aNode->IsContent()) {
    return false;
  }

  RefPtr<dom::HTMLAnchorElement> anchor =
      dom::HTMLAnchorElement::FromNodeOrNull(aNode->AsContent());
  if (!anchor) {
    return false;
  }

  nsAutoString tmpText;
  anchor->GetHref(tmpText);
  return !tmpText.IsEmpty();
}

bool HTMLEditUtils::IsNamedAnchor(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  if (!aNode->IsHTMLElement(nsGkAtoms::a)) {
    return false;
  }

  nsAutoString text;
  return aNode->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name,
                                     text) &&
         !text.IsEmpty();
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
bool HTMLEditUtils::IsMailCite(nsINode* aNode) {
  MOZ_ASSERT(aNode);

  // don't ask me why, but our html mailcites are id'd by "type=cite"...
  if (aNode->IsElement() &&
      aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                      u"cite"_ns, eIgnoreCase)) {
    return true;
  }

  // ... but our plaintext mailcites by "_moz_quote=true".  go figure.
  if (aNode->IsElement() &&
      aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mozquote,
                                      u"true"_ns, eIgnoreCase)) {
    return true;
  }

  return false;
}

/**
 * IsFormWidget() returns true if aNode is a form widget of some kind.
 */
bool HTMLEditUtils::IsFormWidget(nsINode* aNode) {
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

bool HTMLEditUtils::IsVisibleTextNode(Text& aText,
                                      Element* aEditingHost /* = nullptr */) {
  if (!aText.TextDataLength()) {
    return false;
  }

  if (!aText.TextIsOnlyWhitespace()) {
    return true;
  }

  if (!aEditingHost) {
    aEditingHost = HTMLEditUtils::
        GetInclusiveAncestorEditableBlockElementOrInlineEditingHost(aText);
  }
  WSScanResult nextWSScanResult =
      WSRunScanner::ScanNextVisibleNodeOrBlockBoundary(
          aEditingHost, EditorRawDOMPoint(&aText, 0));
  return nextWSScanResult.InNormalWhiteSpacesOrText() &&
         nextWSScanResult.TextPtr() == &aText;
}

bool HTMLEditUtils::IsInVisibleTextFrames(nsPresContext* aPresContext,
                                          Text& aText) {
  MOZ_ASSERT(aPresContext);

  nsIFrame* frame = aText.GetPrimaryFrame();
  if (!frame) {
    return false;
  }

  nsCOMPtr<nsISelectionController> selectionController;
  nsresult rv = frame->GetSelectionController(
      aPresContext, getter_AddRefs(selectionController));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsIFrame::GetSelectionController() failed");
  if (NS_FAILED(rv) || !selectionController) {
    return false;
  }

  if (!aText.TextDataLength()) {
    return false;
  }

  // Ask the selection controller for information about whether any of the
  // data in the node is really rendered.  This is really something that
  // frames know about, but we aren't supposed to talk to frames.  So we put
  // a call in the selection controller interface, since it's already in bed
  // with frames anyway.  (This is a fix for bug 22227, and a partial fix for
  // bug 46209.)
  bool isVisible = false;
  rv = selectionController->CheckVisibilityContent(
      &aText, 0, aText.TextDataLength(), &isVisible);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "nsISelectionController::CheckVisibilityContent() failed");
  return NS_SUCCEEDED(rv) && isVisible;
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
// noscript, object, ol, p, pre, table, section, summary, ul
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
    ELEM(menuitem, false, false, GROUP_NONE, GROUP_NONE),
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

bool HTMLEditUtils::IsContainerNode(nsHTMLTag aTagId) {
  NS_ASSERTION(aTagId > eHTMLTag_unknown && aTagId <= eHTMLTag_userdefined,
               "aTagId out of range!");

  return kElements[aTagId - 1].mIsContainer;
}

bool HTMLEditUtils::IsNonListSingleLineContainer(nsINode& aNode) {
  return aNode.IsAnyOfHTMLElements(
      nsGkAtoms::address, nsGkAtoms::div, nsGkAtoms::h1, nsGkAtoms::h2,
      nsGkAtoms::h3, nsGkAtoms::h4, nsGkAtoms::h5, nsGkAtoms::h6,
      nsGkAtoms::listing, nsGkAtoms::p, nsGkAtoms::pre, nsGkAtoms::xmp);
}

bool HTMLEditUtils::IsSingleLineContainer(nsINode& aNode) {
  return IsNonListSingleLineContainer(aNode) ||
         aNode.IsAnyOfHTMLElements(nsGkAtoms::li, nsGkAtoms::dt, nsGkAtoms::dd);
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

  // First, look for previous content.
  nsIContent* previousContent = aContent.GetPreviousSibling();
  if (!previousContent) {
    if (!aContent.GetParentElement()) {
      return EditorDOMPointType();
    }
    nsIContent* inclusiveAncestor = &aContent;
    for (Element* parentElement : aContent.AncestorsOfType<Element>()) {
      previousContent = parentElement->GetPreviousSibling();
      if (!previousContent &&
          (parentElement == aAncestorLimiter ||
           !HTMLEditUtils::IsSimplyEditableNode(*parentElement) ||
           !HTMLEditUtils::CanCrossContentBoundary(*parentElement,
                                                   aHowToTreatTableBoundary))) {
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

  // First, look for next content.
  nsIContent* nextContent = aContent.GetNextSibling();
  if (!nextContent) {
    if (!aContent.GetParentElement()) {
      return EditorDOMPointType();
    }
    nsIContent* inclusiveAncestor = &aContent;
    for (Element* parentElement : aContent.AncestorsOfType<Element>()) {
      // End of the parent element is a next editable point if it's an
      // element which is not a table structure element.
      if (!HTMLEditUtils::IsAnyTableElement(parentElement) ||
          HTMLEditUtils::IsTableCellOrCaption(*parentElement)) {
        inclusiveAncestor = parentElement;
      }

      nextContent = parentElement->GetNextSibling();
      if (!nextContent &&
          (parentElement == aAncestorLimiter ||
           !HTMLEditUtils::IsSimplyEditableNode(*parentElement) ||
           !HTMLEditUtils::CanCrossContentBoundary(*parentElement,
                                                   aHowToTreatTableBoundary))) {
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
Element*
HTMLEditUtils::GetInclusiveAncestorEditableBlockElementOrInlineEditingHost(
    nsIContent& aContent) {
  MOZ_ASSERT(EditorUtils::IsEditableContent(aContent, EditorType::HTML));
  Element* maybeInlineEditingHost = nullptr;
  for (Element* element : aContent.InclusiveAncestorsOfType<Element>()) {
    if (!EditorUtils::IsEditableContent(*element, EditorType::HTML)) {
      return maybeInlineEditingHost;
    }
    if (HTMLEditUtils::IsBlockElement(*element)) {
      return element;
    }
    maybeInlineEditingHost = element;
  }
  return maybeInlineEditingHost;
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

}  // namespace mozilla
