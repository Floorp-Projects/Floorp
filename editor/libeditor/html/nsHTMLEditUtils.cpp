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

#include "nsHTMLEditUtils.h"
#include "nsTextEditUtils.h"

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsEditor.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLAnchorElement.h"

///////////////////////////////////////////////////////////////////////////
//                  
PRBool 
nsHTMLEditUtils::IsBig(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("big"));
}


///////////////////////////////////////////////////////////////////////////
//                  
PRBool 
nsHTMLEditUtils::IsSmall(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("small"));
}


/********************************************************
 *  helper methods from nsHTMLEditRules
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// IsHeader: true if node an html header
//                  
PRBool 
nsHTMLEditUtils::IsHeader(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsHeader");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  ToLowerCase(tag);
  if ( (tag.Equals(NS_LITERAL_STRING("h1"))) ||
       (tag.Equals(NS_LITERAL_STRING("h2"))) ||
       (tag.Equals(NS_LITERAL_STRING("h3"))) ||
       (tag.Equals(NS_LITERAL_STRING("h4"))) ||
       (tag.Equals(NS_LITERAL_STRING("h5"))) ||
       (tag.Equals(NS_LITERAL_STRING("h6"))) )
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsParagraph: true if node an html paragraph
//                  
PRBool 
nsHTMLEditUtils::IsParagraph(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("p"));
}


///////////////////////////////////////////////////////////////////////////
// IsHR: true if node an horizontal rule
//                  
PRBool 
nsHTMLEditUtils::IsHR(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("hr"));
}


///////////////////////////////////////////////////////////////////////////
// IsListItem: true if node an html list item
//                  
PRBool 
nsHTMLEditUtils::IsListItem(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsListItem");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  ToLowerCase(tag);
  if (tag.Equals(NS_LITERAL_STRING("li")) ||
      tag.Equals(NS_LITERAL_STRING("dd")) ||
      tag.Equals(NS_LITERAL_STRING("dt")))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsTableElement: true if node an html table, td, tr, ...
//                  
PRBool 
nsHTMLEditUtils::IsTableElement(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTableElement");
  nsAutoString tagName;
  nsEditor::GetTagString(node,tagName);
  if (tagName.Equals(NS_LITERAL_STRING("table")) || tagName.Equals(NS_LITERAL_STRING("tr")) || 
      tagName.Equals(NS_LITERAL_STRING("td"))    || tagName.Equals(NS_LITERAL_STRING("th")) ||
      tagName.Equals(NS_LITERAL_STRING("thead")) || tagName.Equals(NS_LITERAL_STRING("tfoot")) ||
      tagName.Equals(NS_LITERAL_STRING("tbody")) || tagName.Equals(NS_LITERAL_STRING("caption")))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsTable: true if node an html table
//                  
PRBool 
nsHTMLEditUtils::IsTable(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("table"));
}

///////////////////////////////////////////////////////////////////////////
// IsTableRow: true if node an html tr
//                  
PRBool 
nsHTMLEditUtils::IsTableRow(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("tr"));
}


///////////////////////////////////////////////////////////////////////////
// IsTableCell: true if node an html td or th
//                  
PRBool 
nsHTMLEditUtils::IsTableCell(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsTableCell");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  ToLowerCase(tag);
  if (tag.Equals(NS_LITERAL_STRING("td")) || tag.Equals(NS_LITERAL_STRING("th")))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsTableCell: true if node an html td or th
//                  
PRBool 
nsHTMLEditUtils::IsTableCellOrCaption(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsTableCell");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  ToLowerCase(tag);
  if (tag.Equals(NS_LITERAL_STRING("td")) || 
      tag.Equals(NS_LITERAL_STRING("th")) ||
      tag.Equals(NS_LITERAL_STRING("caption")) )
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsList: true if node an html list
//                  
PRBool 
nsHTMLEditUtils::IsList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  ToLowerCase(tag);
  if ( (tag.Equals(NS_LITERAL_STRING("dl"))) ||
       (tag.Equals(NS_LITERAL_STRING("ol"))) ||
       (tag.Equals(NS_LITERAL_STRING("ul"))) )
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsOrderedList: true if node an html ordered list
//                  
PRBool 
nsHTMLEditUtils::IsOrderedList(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("ol"));
}


///////////////////////////////////////////////////////////////////////////
// IsUnorderedList: true if node an html unordered list
//                  
PRBool 
nsHTMLEditUtils::IsUnorderedList(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("ul"));
}


///////////////////////////////////////////////////////////////////////////
// IsBlockquote: true if node an html blockquote node
//                  
PRBool 
nsHTMLEditUtils::IsBlockquote(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("blockquote"));
}


///////////////////////////////////////////////////////////////////////////
// IsPre: true if node an html pre node
//                  
PRBool 
nsHTMLEditUtils::IsPre(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("pre"));
}


///////////////////////////////////////////////////////////////////////////
// IsAddress: true if node an html address node
//                  
PRBool 
nsHTMLEditUtils::IsAddress(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("address"));
}


///////////////////////////////////////////////////////////////////////////
// IsImage: true if node an html image node
//                  
PRBool 
nsHTMLEditUtils::IsImage(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("img"));
}

PRBool 
nsHTMLEditUtils::IsLink(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
  if (anchor)
  {
    nsAutoString tmpText;
    if (NS_SUCCEEDED(anchor->GetHref(tmpText)) && tmpText.get() && tmpText.Length() != 0)
      return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
nsHTMLEditUtils::IsNamedAnchor(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
  if (anchor)
  {
    nsAutoString tmpText;
    if (NS_SUCCEEDED(anchor->GetName(tmpText)) && tmpText.get() && tmpText.Length() != 0)
      return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsDiv: true if node an html div node
//                  
PRBool 
nsHTMLEditUtils::IsDiv(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("div"));
}


///////////////////////////////////////////////////////////////////////////
// IsMozDiv: true if node an html div node with type = _moz
//                  
PRBool 
nsHTMLEditUtils::IsMozDiv(nsIDOMNode *node)
{
  if (IsDiv(node) && nsTextEditUtils::HasMozAttr(node)) return PR_TRUE;
  return PR_FALSE;
}



///////////////////////////////////////////////////////////////////////////
// IsMailCite: true if node an html blockquote with type=cite
//                  
PRBool 
nsHTMLEditUtils::IsMailCite(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsMailCite");
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(node);
  if (!elem) return PR_FALSE;
  nsAutoString attrName (NS_LITERAL_STRING("type")); 
  
  // don't ask me why, but our html mailcites are id'd by "type=cite"...
  nsAutoString attrVal;
  nsresult res = elem->GetAttribute(attrName, attrVal);
  ToLowerCase(attrVal);
  if (NS_SUCCEEDED(res))
  {
    if (attrVal.Equals(NS_LITERAL_STRING("cite")))
      return PR_TRUE;
  }

  // ... but our plaintext mailcites by "_moz_quote=true".  go figure.
  attrName.Assign(NS_LITERAL_STRING("_moz_quote"));
  res = elem->GetAttribute(attrName, attrVal);
  if (NS_SUCCEEDED(res))
  {
    ToLowerCase(attrVal);
    if (attrVal.Equals(NS_LITERAL_STRING("true")))
      return PR_TRUE;
  }

  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsFormWidget: true if node is a form widget of some kind
//                  
PRBool 
nsHTMLEditUtils::IsFormWidget(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsFormWidget");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  ToLowerCase(tag);
  if (tag.Equals(NS_LITERAL_STRING("textarea")) || 
      tag.Equals(NS_LITERAL_STRING("select")) ||
      tag.Equals(NS_LITERAL_STRING("button")) ||
      tag.Equals(NS_LITERAL_STRING("input")) )
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsHTMLEditUtils::SupportsAlignAttr(nsIDOMNode * aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditUtils::SupportsAlignAttr");
  nsAutoString tag;
  nsEditor::GetTagString(aNode, tag);
  ToLowerCase(tag);
  if (tag.Equals(NS_LITERAL_STRING("hr")) ||
      tag.Equals(NS_LITERAL_STRING("table")) ||
      tag.Equals(NS_LITERAL_STRING("tbody")) ||
      tag.Equals(NS_LITERAL_STRING("tfoot")) ||
      tag.Equals(NS_LITERAL_STRING("thead")) ||
      tag.Equals(NS_LITERAL_STRING("tr")) ||
      tag.Equals(NS_LITERAL_STRING("td")) ||
      tag.Equals(NS_LITERAL_STRING("th")) ||
      tag.Equals(NS_LITERAL_STRING("div")) ||
      tag.Equals(NS_LITERAL_STRING("p")) ||
      tag.Equals(NS_LITERAL_STRING("h1")) ||
      tag.Equals(NS_LITERAL_STRING("h2")) ||
      tag.Equals(NS_LITERAL_STRING("h3")) ||
      tag.Equals(NS_LITERAL_STRING("h4")) ||
      tag.Equals(NS_LITERAL_STRING("h5")) ||
      tag.Equals(NS_LITERAL_STRING("h6"))) {
    return PR_TRUE;
  }
  return PR_FALSE;
}
