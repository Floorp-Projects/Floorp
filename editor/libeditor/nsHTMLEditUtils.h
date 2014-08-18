/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLEditUtils_h__
#define nsHTMLEditUtils_h__


class nsIDOMNode;
class nsINode;

class nsHTMLEditUtils
{
public:
  // from nsTextEditRules:
  static bool IsBig(nsIDOMNode *aNode);
  static bool IsSmall(nsIDOMNode *aNode);

  // from nsHTMLEditRules:
  static bool IsInlineStyle(nsINode* aNode);
  static bool IsInlineStyle(nsIDOMNode *aNode);
  static bool IsFormatNode(nsINode* aNode);
  static bool IsFormatNode(nsIDOMNode *aNode);
  static bool IsNodeThatCanOutdent(nsIDOMNode *aNode);
  static bool IsHeader(nsIDOMNode *aNode);
  static bool IsParagraph(nsIDOMNode *aNode);
  static bool IsHR(nsIDOMNode *aNode);
  static bool IsListItem(nsINode* aNode);
  static bool IsListItem(nsIDOMNode *aNode);
  static bool IsTable(nsIDOMNode *aNode);
  static bool IsTable(nsINode* aNode);
  static bool IsTableRow(nsIDOMNode *aNode);
  static bool IsTableElement(nsINode* aNode);
  static bool IsTableElement(nsIDOMNode *aNode);
  static bool IsTableElementButNotTable(nsINode* aNode);
  static bool IsTableElementButNotTable(nsIDOMNode *aNode);
  static bool IsTableCell(nsINode* node);
  static bool IsTableCell(nsIDOMNode *aNode);
  static bool IsTableCellOrCaption(nsIDOMNode *aNode);
  static bool IsList(nsINode* aNode);
  static bool IsList(nsIDOMNode *aNode);
  static bool IsOrderedList(nsIDOMNode *aNode);
  static bool IsUnorderedList(nsIDOMNode *aNode);
  static bool IsBlockquote(nsIDOMNode *aNode);
  static bool IsPre(nsIDOMNode *aNode);
  static bool IsAnchor(nsIDOMNode *aNode);
  static bool IsImage(nsIDOMNode *aNode);
  static bool IsLink(nsIDOMNode *aNode);
  static bool IsLink(nsINode* aNode);
  static bool IsNamedAnchor(nsINode* aNode);
  static bool IsNamedAnchor(nsIDOMNode *aNode);
  static bool IsDiv(nsIDOMNode *aNode);
  static bool IsMozDiv(nsIDOMNode *aNode);
  static bool IsMailCite(nsINode* aNode);
  static bool IsMailCite(nsIDOMNode *aNode);
  static bool IsFormWidget(nsINode* aNode);
  static bool IsFormWidget(nsIDOMNode *aNode);
  static bool SupportsAlignAttr(nsIDOMNode *aNode);
  static bool CanContain(int32_t aParent, int32_t aChild);
  static bool IsContainer(int32_t aTag);
};

#endif /* nsHTMLEditUtils_h__ */

