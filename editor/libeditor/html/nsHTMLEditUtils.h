/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHTMLEditUtils_h__
#define nsHTMLEditUtils_h__

#include "prtypes.h"  // for PRBool
#include "nsError.h"  // for nsresult
class nsIEditor;
class nsIDOMNode;

class nsHTMLEditUtils
{
public:
  // from nsTextEditRules:
  static PRBool IsBig(nsIDOMNode *aNode);
  static PRBool IsSmall(nsIDOMNode *aNode);

  // from nsHTMLEditRules:
  static PRBool IsHeader(nsIDOMNode *aNode);
  static PRBool IsParagraph(nsIDOMNode *aNode);
  static PRBool IsListItem(nsIDOMNode *aNode);
  static PRBool IsTable(nsIDOMNode *aNode);
  static PRBool IsTableRow(nsIDOMNode *aNode);
  static PRBool IsTableElement(nsIDOMNode *aNode);
  static PRBool IsTableCell(nsIDOMNode *aNode);
  static PRBool IsTableCellOrCaption(nsIDOMNode *aNode);
  static PRBool IsList(nsIDOMNode *aNode);
  static PRBool IsOrderedList(nsIDOMNode *aNode);
  static PRBool IsUnorderedList(nsIDOMNode *aNode);
  static PRBool IsDefinitionList(nsIDOMNode *aNode);
  static PRBool IsBlockquote(nsIDOMNode *aNode);
  static PRBool IsPre(nsIDOMNode *aNode);
  static PRBool IsAddress(nsIDOMNode *aNode);
  static PRBool IsAnchor(nsIDOMNode *aNode);
  static PRBool IsImage(nsIDOMNode *aNode);
  static PRBool IsLink(nsIDOMNode *aNode);
  static PRBool IsNamedAnchor(nsIDOMNode *aNode);
  static PRBool IsDiv(nsIDOMNode *aNode);
  static PRBool IsNormalDiv(nsIDOMNode *aNode);
  static PRBool IsMozDiv(nsIDOMNode *aNode);
  static PRBool IsMailCite(nsIDOMNode *aNode);
  static PRBool IsTextarea(nsIDOMNode *aNode);
  static PRBool IsMap(nsIDOMNode *aNode);
  static PRBool IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent);
  
  static PRBool IsLeafNode(nsIDOMNode *aNode);
  static PRBool SupportsAlignAttr(nsIDOMNode *aNode);

};

#endif /* nsHTMLEditUtils_h__ */

