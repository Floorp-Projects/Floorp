/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditUtils.h"

#include "TextEditUtils.h"              // for TextEditUtils
#include "mozilla/ArrayUtils.h"         // for ArrayLength
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/EditorBase.h"         // for EditorBase
#include "mozilla/dom/Element.h"        // for Element, nsINode
#include "nsAString.h"                  // for nsAString::IsEmpty
#include "nsCOMPtr.h"                   // for nsCOMPtr, operator==, etc.
#include "nsCaseTreatment.h"
#include "nsDebug.h"                    // for NS_PRECONDITION, etc.
#include "nsError.h"                    // for NS_SUCCEEDED
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::a, etc.
#include "nsHTMLTags.h"
#include "nsIAtom.h"                    // for nsIAtom
#include "nsIDOMHTMLAnchorElement.h"    // for nsIDOMHTMLAnchorElement
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsString.h"                   // for nsAutoString

namespace mozilla {

/**
 * IsInlineStyle() returns true if aNode is an inline style.
 */
bool
HTMLEditUtils::IsInlineStyle(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsInlineStyle(node);
}

bool
HTMLEditUtils::IsInlineStyle(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::b,
                                    nsGkAtoms::i,
                                    nsGkAtoms::u,
                                    nsGkAtoms::tt,
                                    nsGkAtoms::s,
                                    nsGkAtoms::strike,
                                    nsGkAtoms::big,
                                    nsGkAtoms::small,
                                    nsGkAtoms::sub,
                                    nsGkAtoms::sup,
                                    nsGkAtoms::font);
}

/**
 * IsFormatNode() returns true if aNode is a format node.
 */
bool
HTMLEditUtils::IsFormatNode(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsFormatNode(node);
}

bool
HTMLEditUtils::IsFormatNode(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::p,
                                    nsGkAtoms::pre,
                                    nsGkAtoms::h1,
                                    nsGkAtoms::h2,
                                    nsGkAtoms::h3,
                                    nsGkAtoms::h4,
                                    nsGkAtoms::h5,
                                    nsGkAtoms::h6,
                                    nsGkAtoms::address);
}

/**
 * IsNodeThatCanOutdent() returns true if aNode is a list, list item or
 * blockquote.
 */
bool
HTMLEditUtils::IsNodeThatCanOutdent(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsIAtom> nodeAtom = EditorBase::GetTag(aNode);
  return (nodeAtom == nsGkAtoms::ul)
      || (nodeAtom == nsGkAtoms::ol)
      || (nodeAtom == nsGkAtoms::dl)
      || (nodeAtom == nsGkAtoms::li)
      || (nodeAtom == nsGkAtoms::dd)
      || (nodeAtom == nsGkAtoms::dt)
      || (nodeAtom == nsGkAtoms::blockquote);
}

/**
 * IsHeader() returns true if aNode is an html header.
 */
bool
HTMLEditUtils::IsHeader(nsINode& aNode)
{
  return aNode.IsAnyOfHTMLElements(nsGkAtoms::h1,
                                   nsGkAtoms::h2,
                                   nsGkAtoms::h3,
                                   nsGkAtoms::h4,
                                   nsGkAtoms::h5,
                                   nsGkAtoms::h6);
}

bool
HTMLEditUtils::IsHeader(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  MOZ_ASSERT(node);
  return IsHeader(*node);
}

/**
 * IsParagraph() returns true if aNode is an html paragraph.
 */
bool
HTMLEditUtils::IsParagraph(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::p);
}

/**
 * IsListItem() returns true if aNode is an html list item.
 */
bool
HTMLEditUtils::IsListItem(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsListItem(node);
}

bool
HTMLEditUtils::IsListItem(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::li,
                                    nsGkAtoms::dd,
                                    nsGkAtoms::dt);
}

/**
 * IsTableElement() returns true if aNode is an html table, td, tr, ...
 */
bool
HTMLEditUtils::IsTableElement(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsTableElement(node);
}

bool
HTMLEditUtils::IsTableElement(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::table,
                                    nsGkAtoms::tr,
                                    nsGkAtoms::td,
                                    nsGkAtoms::th,
                                    nsGkAtoms::thead,
                                    nsGkAtoms::tfoot,
                                    nsGkAtoms::tbody,
                                    nsGkAtoms::caption);
}

/**
 * IsTableElementButNotTable() returns true if aNode is an html td, tr, ...
 * (doesn't include table)
 */
bool
HTMLEditUtils::IsTableElementButNotTable(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsTableElementButNotTable(node);
}

bool
HTMLEditUtils::IsTableElementButNotTable(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::tr,
                                     nsGkAtoms::td,
                                     nsGkAtoms::th,
                                     nsGkAtoms::thead,
                                     nsGkAtoms::tfoot,
                                     nsGkAtoms::tbody,
                                     nsGkAtoms::caption);
}

/**
 * IsTable() returns true if aNode is an html table.
 */
bool
HTMLEditUtils::IsTable(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::table);
}

bool
HTMLEditUtils::IsTable(nsINode* aNode)
{
  return aNode && aNode->IsHTMLElement(nsGkAtoms::table);
}

/**
 * IsTableRow() returns true if aNode is an html tr.
 */
bool
HTMLEditUtils::IsTableRow(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::tr);
}

/**
 * IsTableCell() returns true if aNode is an html td or th.
 */
bool
HTMLEditUtils::IsTableCell(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsTableCell(node);
}

bool
HTMLEditUtils::IsTableCell(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th);
}

/**
 * IsTableCellOrCaption() returns true if aNode is an html td or th or caption.
 */
bool
HTMLEditUtils::IsTableCellOrCaption(nsINode& aNode)
{
  return aNode.IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th,
                                   nsGkAtoms::caption);
}

/**
 * IsList() returns true if aNode is an html list.
 */
bool
HTMLEditUtils::IsList(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsList(node);
}

bool
HTMLEditUtils::IsList(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::ul,
                                    nsGkAtoms::ol,
                                    nsGkAtoms::dl);
}

/**
 * IsOrderedList() returns true if aNode is an html ordered list.
 */
bool
HTMLEditUtils::IsOrderedList(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::ol);
}


/**
 * IsUnorderedList() returns true if aNode is an html unordered list.
 */
bool
HTMLEditUtils::IsUnorderedList(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::ul);
}

/**
 * IsBlockquote() returns true if aNode is an html blockquote node.
 */
bool
HTMLEditUtils::IsBlockquote(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::blockquote);
}

/**
 * IsPre() returns true if aNode is an html pre node.
 */
bool
HTMLEditUtils::IsPre(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::pre);
}

/**
 * IsImage() returns true if aNode is an html image node.
 */
bool
HTMLEditUtils::IsImage(nsINode* aNode)
{
  return aNode && aNode->IsHTMLElement(nsGkAtoms::img);
}

bool
HTMLEditUtils::IsImage(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::img);
}

bool
HTMLEditUtils::IsLink(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsLink(node);
}

bool
HTMLEditUtils::IsLink(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
  if (anchor) {
    nsAutoString tmpText;
    if (NS_SUCCEEDED(anchor->GetHref(tmpText)) && !tmpText.IsEmpty()) {
      return true;
    }
  }
  return false;
}

bool
HTMLEditUtils::IsNamedAnchor(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsNamedAnchor(node);
}

bool
HTMLEditUtils::IsNamedAnchor(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (!aNode->IsHTMLElement(nsGkAtoms::a)) {
    return false;
  }

  nsAutoString text;
  return aNode->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name,
                                     text) && !text.IsEmpty();
}

/**
 * IsDiv() returns true if aNode is an html div node.
 */
bool
HTMLEditUtils::IsDiv(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::div);
}

/**
 * IsMozDiv() returns true if aNode is an html div node with |type = _moz|.
 */
bool
HTMLEditUtils::IsMozDiv(nsIDOMNode* aNode)
{
  return IsDiv(aNode) && TextEditUtils::HasMozAttr(aNode);
}

bool
HTMLEditUtils::IsMozDiv(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsHTMLElement(nsGkAtoms::div) &&
         TextEditUtils::HasMozAttr(GetAsDOMNode(aNode));
}

/**
 * IsMailCite() returns true if aNode is an html blockquote with |type=cite|.
 */
bool
HTMLEditUtils::IsMailCite(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsMailCite(node);
}

bool
HTMLEditUtils::IsMailCite(nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  // don't ask me why, but our html mailcites are id'd by "type=cite"...
  if (aNode->IsElement() &&
      aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                      NS_LITERAL_STRING("cite"),
                                      eIgnoreCase)) {
    return true;
  }

  // ... but our plaintext mailcites by "_moz_quote=true".  go figure.
  if (aNode->IsElement() &&
      aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mozquote,
                                      NS_LITERAL_STRING("true"),
                                      eIgnoreCase)) {
    return true;
  }

  return false;
}

/**
 * IsFormWidget() returns true if aNode is a form widget of some kind.
 */
bool
HTMLEditUtils::IsFormWidget(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return node && IsFormWidget(node);
}

bool
HTMLEditUtils::IsFormWidget(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsAnyOfHTMLElements(nsGkAtoms::textarea,
                                    nsGkAtoms::select,
                                    nsGkAtoms::button,
                                    nsGkAtoms::output,
                                    nsGkAtoms::keygen,
                                    nsGkAtoms::progress,
                                    nsGkAtoms::meter,
                                    nsGkAtoms::input);
}

bool
HTMLEditUtils::SupportsAlignAttr(nsINode& aNode)
{
  return aNode.IsAnyOfHTMLElements(nsGkAtoms::hr,
                                   nsGkAtoms::table,
                                   nsGkAtoms::tbody,
                                   nsGkAtoms::tfoot,
                                   nsGkAtoms::thead,
                                   nsGkAtoms::tr,
                                   nsGkAtoms::td,
                                   nsGkAtoms::th,
                                   nsGkAtoms::div,
                                   nsGkAtoms::p,
                                   nsGkAtoms::h1,
                                   nsGkAtoms::h2,
                                   nsGkAtoms::h3,
                                   nsGkAtoms::h4,
                                   nsGkAtoms::h5,
                                   nsGkAtoms::h6);
}

// We use bitmasks to test containment of elements. Elements are marked to be
// in certain groups by setting the mGroup member of the nsElementInfo struct
// to the corresponding GROUP_ values (OR'ed together). Similarly, elements are
// marked to allow containment of certain groups by setting the
// mCanContainGroups member of the nsElementInfo struct to the corresponding
// GROUP_ values (OR'ed together).
// Testing containment then simply consists of checking whether the
// mCanContainGroups bitmask of an element and the mGroup bitmask of a
// potential child overlap.

#define GROUP_NONE             0

// body, head, html
#define GROUP_TOPLEVEL         (1 << 1)

// base, link, meta, script, style, title
#define GROUP_HEAD_CONTENT     (1 << 2)

// b, big, i, s, small, strike, tt, u
#define GROUP_FONTSTYLE        (1 << 3)

// abbr, acronym, cite, code, datalist, del, dfn, em, ins, kbd, mark, rb, rp
// rt, rtc, ruby, samp, strong, var
#define GROUP_PHRASE           (1 << 4)

// a, applet, basefont, bdo, br, font, iframe, img, map, meter, object, output,
// picture, progress, q, script, span, sub, sup
#define GROUP_SPECIAL          (1 << 5)

// button, form, input, label, select, textarea
#define GROUP_FORMCONTROL      (1 << 6)

// address, applet, article, aside, blockquote, button, center, del, details,
// dialog, dir, div, dl, fieldset, figure, footer, form, h1, h2, h3, h4, h5,
// h6, header, hgroup, hr, iframe, ins, main, map, menu, nav, noframes,
// noscript, object, ol, p, pre, table, section, summary, ul
#define GROUP_BLOCK            (1 << 7)

// frame, frameset
#define GROUP_FRAME            (1 << 8)

// col, tbody
#define GROUP_TABLE_CONTENT    (1 << 9)

// tr
#define GROUP_TBODY_CONTENT    (1 << 10)

// td, th
#define GROUP_TR_CONTENT       (1 << 11)

// col
#define GROUP_COLGROUP_CONTENT (1 << 12)

// param
#define GROUP_OBJECT_CONTENT   (1 << 13)

// li
#define GROUP_LI               (1 << 14)

// area
#define GROUP_MAP_CONTENT      (1 << 15)

// optgroup, option
#define GROUP_SELECT_CONTENT   (1 << 16)

// option
#define GROUP_OPTIONS          (1 << 17)

// dd, dt
#define GROUP_DL_CONTENT       (1 << 18)

// p
#define GROUP_P                (1 << 19)

// text, whitespace, newline, comment
#define GROUP_LEAF             (1 << 20)

// XXX This is because the editor does sublists illegally.
// ol, ul
#define GROUP_OL_UL            (1 << 21)

// h1, h2, h3, h4, h5, h6
#define GROUP_HEADING          (1 << 22)

// figcaption
#define GROUP_FIGCAPTION       (1 << 23)

// picture members (img, source)
#define GROUP_PICTURE_CONTENT  (1 << 24)

#define GROUP_INLINE_ELEMENT \
  (GROUP_FONTSTYLE | GROUP_PHRASE | GROUP_SPECIAL | GROUP_FORMCONTROL | \
   GROUP_LEAF)

#define GROUP_FLOW_ELEMENT (GROUP_INLINE_ELEMENT | GROUP_BLOCK)

struct ElementInfo final
{
#ifdef DEBUG
  eHTMLTags mTag;
#endif
  uint32_t mGroup;
  uint32_t mCanContainGroups;
  bool mIsContainer;
  bool mCanContainSelf;
};

#ifdef DEBUG
#define ELEM(_tag, _isContainer, _canContainSelf, _group, _canContainGroups) \
  { eHTMLTag_##_tag, _group, _canContainGroups, _isContainer, _canContainSelf }
#else
#define ELEM(_tag, _isContainer, _canContainSelf, _group, _canContainGroups) \
  { _group, _canContainGroups, _isContainer, _canContainSelf }
#endif

static const ElementInfo kElements[eHTMLTag_userdefined] = {
  ELEM(a, true, false, GROUP_SPECIAL, GROUP_INLINE_ELEMENT),
  ELEM(abbr, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
  ELEM(acronym, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
  ELEM(address, true, true, GROUP_BLOCK,
       GROUP_INLINE_ELEMENT | GROUP_P),
  // While applet is no longer a valid tag, removing it here breaks the editor
  // (compiles, but causes many tests to fail in odd ways). This list is tracked
  // against the main HTML Tag list, so any changes will require more than just
  // removing entries.
  ELEM(applet, true, true, GROUP_SPECIAL | GROUP_BLOCK,
       GROUP_FLOW_ELEMENT | GROUP_OBJECT_CONTENT),
  ELEM(area, false, false, GROUP_MAP_CONTENT, GROUP_NONE),
  ELEM(article, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(aside, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(audio, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(b, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
  ELEM(base, false, false, GROUP_HEAD_CONTENT, GROUP_NONE),
  ELEM(basefont, false, false, GROUP_SPECIAL, GROUP_NONE),
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
  ELEM(content, true, false, GROUP_NONE, GROUP_INLINE_ELEMENT),
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
  ELEM(h1, true, false, GROUP_BLOCK | GROUP_HEADING,
       GROUP_INLINE_ELEMENT),
  ELEM(h2, true, false, GROUP_BLOCK | GROUP_HEADING,
       GROUP_INLINE_ELEMENT),
  ELEM(h3, true, false, GROUP_BLOCK | GROUP_HEADING,
       GROUP_INLINE_ELEMENT),
  ELEM(h4, true, false, GROUP_BLOCK | GROUP_HEADING,
       GROUP_INLINE_ELEMENT),
  ELEM(h5, true, false, GROUP_BLOCK | GROUP_HEADING,
       GROUP_INLINE_ELEMENT),
  ELEM(h6, true, false, GROUP_BLOCK | GROUP_HEADING,
       GROUP_INLINE_ELEMENT),
  ELEM(head, true, false, GROUP_TOPLEVEL, GROUP_HEAD_CONTENT),
  ELEM(header, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(hgroup, true, false, GROUP_BLOCK, GROUP_HEADING),
  ELEM(hr, false, false, GROUP_BLOCK, GROUP_NONE),
  ELEM(html, true, false, GROUP_TOPLEVEL, GROUP_TOPLEVEL),
  ELEM(i, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
  ELEM(iframe, true, true, GROUP_SPECIAL | GROUP_BLOCK,
       GROUP_FLOW_ELEMENT),
  ELEM(image, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(img, false, false, GROUP_SPECIAL | GROUP_PICTURE_CONTENT, GROUP_NONE),
  ELEM(input, false, false, GROUP_FORMCONTROL, GROUP_NONE),
  ELEM(ins, true, true, GROUP_PHRASE | GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(kbd, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
  ELEM(keygen, false, false, GROUP_FORMCONTROL, GROUP_NONE),
  ELEM(label, true, false, GROUP_FORMCONTROL, GROUP_INLINE_ELEMENT),
  ELEM(legend, true, true, GROUP_NONE, GROUP_INLINE_ELEMENT),
  ELEM(li, true, false, GROUP_LI, GROUP_FLOW_ELEMENT),
  ELEM(link, false, false, GROUP_HEAD_CONTENT, GROUP_NONE),
  ELEM(listing, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(main, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(map, true, true, GROUP_SPECIAL, GROUP_BLOCK | GROUP_MAP_CONTENT),
  ELEM(mark, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
  ELEM(marquee, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(menu, true, true, GROUP_BLOCK, GROUP_LI | GROUP_FLOW_ELEMENT),
  ELEM(menuitem, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(meta, false, false, GROUP_HEAD_CONTENT, GROUP_NONE),
  ELEM(meter, true, false, GROUP_SPECIAL, GROUP_FLOW_ELEMENT),
  ELEM(multicol, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(nav, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(nobr, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(noembed, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(noframes, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(noscript, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(object, true, true, GROUP_SPECIAL | GROUP_BLOCK,
       GROUP_FLOW_ELEMENT | GROUP_OBJECT_CONTENT),
  // XXX Can contain self and ul because editor does sublists illegally.
  ELEM(ol, true, true, GROUP_BLOCK | GROUP_OL_UL,
       GROUP_LI | GROUP_OL_UL),
  ELEM(optgroup, true, false, GROUP_SELECT_CONTENT,
       GROUP_OPTIONS),
  ELEM(option, true, false,
       GROUP_SELECT_CONTENT | GROUP_OPTIONS, GROUP_LEAF),
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
  ELEM(script, true, false, GROUP_HEAD_CONTENT | GROUP_SPECIAL,
       GROUP_LEAF),
  ELEM(section, true, true, GROUP_BLOCK, GROUP_FLOW_ELEMENT),
  ELEM(select, true, false, GROUP_FORMCONTROL, GROUP_SELECT_CONTENT),
  ELEM(shadow, true, false, GROUP_NONE, GROUP_INLINE_ELEMENT),
  ELEM(small, true, true, GROUP_FONTSTYLE, GROUP_INLINE_ELEMENT),
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
  ELEM(ul, true, true, GROUP_BLOCK | GROUP_OL_UL,
       GROUP_LI | GROUP_OL_UL),
  ELEM(var, true, true, GROUP_PHRASE, GROUP_INLINE_ELEMENT),
  ELEM(video, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(wbr, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(xmp, false, false, GROUP_NONE, GROUP_NONE),

  // These aren't elements.
  ELEM(text, false, false, GROUP_LEAF, GROUP_NONE),
  ELEM(whitespace, false, false, GROUP_LEAF, GROUP_NONE),
  ELEM(newline, false, false, GROUP_LEAF, GROUP_NONE),
  ELEM(comment, false, false, GROUP_LEAF, GROUP_NONE),
  ELEM(entity, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(doctypeDecl, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(markupDecl, false, false, GROUP_NONE, GROUP_NONE),
  ELEM(instruction, false, false, GROUP_NONE, GROUP_NONE),

  ELEM(userdefined, true, false, GROUP_NONE, GROUP_FLOW_ELEMENT)
};

bool
HTMLEditUtils::CanContain(int32_t aParent, int32_t aChild)
{
  NS_ASSERTION(aParent > eHTMLTag_unknown && aParent <= eHTMLTag_userdefined,
               "aParent out of range!");
  NS_ASSERTION(aChild > eHTMLTag_unknown && aChild <= eHTMLTag_userdefined,
               "aChild out of range!");

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
  if (aParent == eHTMLTag_button) {
    static const eHTMLTags kButtonExcludeKids[] = {
      eHTMLTag_a,
      eHTMLTag_fieldset,
      eHTMLTag_form,
      eHTMLTag_iframe,
      eHTMLTag_input,
      eHTMLTag_select,
      eHTMLTag_textarea
    };

    uint32_t j;
    for (j = 0; j < ArrayLength(kButtonExcludeKids); ++j) {
      if (kButtonExcludeKids[j] == aChild) {
        return false;
      }
    }
  }

  // Deprecated elements.
  if (aChild == eHTMLTag_bgsound) {
    return false;
  }

  // Bug #67007, dont strip userdefined tags.
  if (aChild == eHTMLTag_userdefined) {
    return true;
  }

  const ElementInfo& parent = kElements[aParent - 1];
  if (aParent == aChild) {
    return parent.mCanContainSelf;
  }

  const ElementInfo& child = kElements[aChild - 1];
  return (parent.mCanContainGroups & child.mGroup) != 0;
}

bool
HTMLEditUtils::IsContainer(int32_t aTag)
{
  NS_ASSERTION(aTag > eHTMLTag_unknown && aTag <= eHTMLTag_userdefined,
               "aTag out of range!");

  return kElements[aTag - 1].mIsContainer;
}

} // namespace mozilla
