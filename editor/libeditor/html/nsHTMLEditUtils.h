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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsHTMLEditUtils_h__
#define nsHTMLEditUtils_h__

#include "prtypes.h"  // for bool
#include "nsError.h"  // for nsresult
class nsIEditor;
class nsIDOMNode;

class nsHTMLEditUtils
{
public:
  // from nsTextEditRules:
  static bool IsBig(nsIDOMNode *aNode);
  static bool IsSmall(nsIDOMNode *aNode);

  // from nsHTMLEditRules:
  static bool IsInlineStyle(nsIDOMNode *aNode);
  static bool IsFormatNode(nsIDOMNode *aNode);
  static bool IsNodeThatCanOutdent(nsIDOMNode *aNode);
  static bool IsHeader(nsIDOMNode *aNode);
  static bool IsParagraph(nsIDOMNode *aNode);
  static bool IsHR(nsIDOMNode *aNode);
  static bool IsListItem(nsIDOMNode *aNode);
  static bool IsTable(nsIDOMNode *aNode);
  static bool IsTableRow(nsIDOMNode *aNode);
  static bool IsTableElement(nsIDOMNode *aNode);
  static bool IsTableElementButNotTable(nsIDOMNode *aNode);
  static bool IsTableCell(nsIDOMNode *aNode);
  static bool IsTableCellOrCaption(nsIDOMNode *aNode);
  static bool IsList(nsIDOMNode *aNode);
  static bool IsOrderedList(nsIDOMNode *aNode);
  static bool IsUnorderedList(nsIDOMNode *aNode);
  static bool IsBlockquote(nsIDOMNode *aNode);
  static bool IsPre(nsIDOMNode *aNode);
  static bool IsAddress(nsIDOMNode *aNode);
  static bool IsAnchor(nsIDOMNode *aNode);
  static bool IsImage(nsIDOMNode *aNode);
  static bool IsLink(nsIDOMNode *aNode);
  static bool IsNamedAnchor(nsIDOMNode *aNode);
  static bool IsDiv(nsIDOMNode *aNode);
  static bool IsMozDiv(nsIDOMNode *aNode);
  static bool IsMailCite(nsIDOMNode *aNode);
  static bool IsFormWidget(nsIDOMNode *aNode);
  static bool SupportsAlignAttr(nsIDOMNode *aNode);
  static bool CanContain(PRInt32 aParent, PRInt32 aChild);
  static bool IsContainer(PRInt32 aTag);
};

#endif /* nsHTMLEditUtils_h__ */

