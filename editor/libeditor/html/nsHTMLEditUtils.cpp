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

#include "nsHTMLEditUtils.h"
#include "nsTextEditUtils.h"

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsEditor.h"
#include "nsEditProperty.h"
#include "nsIAtom.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLAnchorElement.h"

///////////////////////////////////////////////////////////////////////////
//                  
PRBool 
nsHTMLEditUtils::IsBig(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::big);
}


///////////////////////////////////////////////////////////////////////////
// IsInlineStyle true if node is an inline style
//                  
PRBool 
nsHTMLEditUtils::IsInlineStyle(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsInlineStyle");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::b)
      || (nodeAtom == nsEditProperty::i)
      || (nodeAtom == nsEditProperty::u)
      || (nodeAtom == nsEditProperty::tt)
      || (nodeAtom == nsEditProperty::s)
      || (nodeAtom == nsEditProperty::strike)
      || (nodeAtom == nsEditProperty::big)
      || (nodeAtom == nsEditProperty::small)
      || (nodeAtom == nsEditProperty::blink)
      || (nodeAtom == nsEditProperty::sub)
      || (nodeAtom == nsEditProperty::sup)
      || (nodeAtom == nsEditProperty::font);
}

///////////////////////////////////////////////////////////////////////////
// IsFormatNode true if node is a format node
// 
PRBool
nsHTMLEditUtils::IsFormatNode(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsFormatNode");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::p)
      || (nodeAtom == nsEditProperty::pre)
      || (nodeAtom == nsEditProperty::h1)
      || (nodeAtom == nsEditProperty::h2)
      || (nodeAtom == nsEditProperty::h3)
      || (nodeAtom == nsEditProperty::h4)
      || (nodeAtom == nsEditProperty::h5)
      || (nodeAtom == nsEditProperty::h6)
      || (nodeAtom == nsEditProperty::address);
}

///////////////////////////////////////////////////////////////////////////
// IsNodeThatCanOutdent true if node is a list, list item, or blockquote      
//
PRBool
nsHTMLEditUtils::IsNodeThatCanOutdent(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsNodeThatCanOutdent");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::ul)
      || (nodeAtom == nsEditProperty::ol)
      || (nodeAtom == nsEditProperty::dl)
      || (nodeAtom == nsEditProperty::li)
      || (nodeAtom == nsEditProperty::dd)
      || (nodeAtom == nsEditProperty::dt)
      || (nodeAtom == nsEditProperty::blockquote);
}

///////////////////////////////////////////////////////////////////////////
//                  
PRBool 
nsHTMLEditUtils::IsSmall(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::small);
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
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::h1)
      || (nodeAtom == nsEditProperty::h2)
      || (nodeAtom == nsEditProperty::h3)
      || (nodeAtom == nsEditProperty::h4)
      || (nodeAtom == nsEditProperty::h5)
      || (nodeAtom == nsEditProperty::h6);
}


///////////////////////////////////////////////////////////////////////////
// IsParagraph: true if node an html paragraph
//                  
PRBool 
nsHTMLEditUtils::IsParagraph(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::p);
}


///////////////////////////////////////////////////////////////////////////
// IsHR: true if node an horizontal rule
//                  
PRBool 
nsHTMLEditUtils::IsHR(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::hr);
}


///////////////////////////////////////////////////////////////////////////
// IsListItem: true if node an html list item
//                  
PRBool 
nsHTMLEditUtils::IsListItem(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsListItem");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::li)
      || (nodeAtom == nsEditProperty::dd)
      || (nodeAtom == nsEditProperty::dt);
}


///////////////////////////////////////////////////////////////////////////
// IsTableElement: true if node an html table, td, tr, ...
//                  
PRBool 
nsHTMLEditUtils::IsTableElement(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTableElement");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::table)
      || (nodeAtom == nsEditProperty::tr)
      || (nodeAtom == nsEditProperty::td)
      || (nodeAtom == nsEditProperty::th)
      || (nodeAtom == nsEditProperty::thead)
      || (nodeAtom == nsEditProperty::tfoot)
      || (nodeAtom == nsEditProperty::tbody)
      || (nodeAtom == nsEditProperty::caption);
}

///////////////////////////////////////////////////////////////////////////
// IsTableElementButNotTable: true if node an html td, tr, ... (doesn't include table)
//                  
PRBool 
nsHTMLEditUtils::IsTableElementButNotTable(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTableElementButNotTable");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::tr)
      || (nodeAtom == nsEditProperty::td)
      || (nodeAtom == nsEditProperty::th)
      || (nodeAtom == nsEditProperty::thead)
      || (nodeAtom == nsEditProperty::tfoot)
      || (nodeAtom == nsEditProperty::tbody)
      || (nodeAtom == nsEditProperty::caption);
}

///////////////////////////////////////////////////////////////////////////
// IsTable: true if node an html table
//                  
PRBool 
nsHTMLEditUtils::IsTable(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::table);
}

///////////////////////////////////////////////////////////////////////////
// IsTableRow: true if node an html tr
//                  
PRBool 
nsHTMLEditUtils::IsTableRow(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::tr);
}


///////////////////////////////////////////////////////////////////////////
// IsTableCell: true if node an html td or th
//                  
PRBool 
nsHTMLEditUtils::IsTableCell(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsTableCell");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::td)
      || (nodeAtom == nsEditProperty::th);
}


///////////////////////////////////////////////////////////////////////////
// IsTableCell: true if node an html td or th
//                  
PRBool 
nsHTMLEditUtils::IsTableCellOrCaption(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsTableCell");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::td)
      || (nodeAtom == nsEditProperty::th)
      || (nodeAtom == nsEditProperty::caption);
}


///////////////////////////////////////////////////////////////////////////
// IsList: true if node an html list
//                  
PRBool 
nsHTMLEditUtils::IsList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsList");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::ul)
      || (nodeAtom == nsEditProperty::ol)
      || (nodeAtom == nsEditProperty::dl);
}


///////////////////////////////////////////////////////////////////////////
// IsOrderedList: true if node an html ordered list
//                  
PRBool 
nsHTMLEditUtils::IsOrderedList(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::ol);
}


///////////////////////////////////////////////////////////////////////////
// IsUnorderedList: true if node an html unordered list
//                  
PRBool 
nsHTMLEditUtils::IsUnorderedList(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::ul);
}


///////////////////////////////////////////////////////////////////////////
// IsBlockquote: true if node an html blockquote node
//                  
PRBool 
nsHTMLEditUtils::IsBlockquote(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::blockquote);
}


///////////////////////////////////////////////////////////////////////////
// IsPre: true if node an html pre node
//                  
PRBool 
nsHTMLEditUtils::IsPre(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::pre);
}


///////////////////////////////////////////////////////////////////////////
// IsAddress: true if node an html address node
//                  
PRBool 
nsHTMLEditUtils::IsAddress(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::address);
}


///////////////////////////////////////////////////////////////////////////
// IsImage: true if node an html image node
//                  
PRBool 
nsHTMLEditUtils::IsImage(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsEditProperty::img);
}

PRBool 
nsHTMLEditUtils::IsLink(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
  if (anchor)
  {
    nsAutoString tmpText;
    if (NS_SUCCEEDED(anchor->GetHref(tmpText)) && !tmpText.IsEmpty())
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
    if (NS_SUCCEEDED(anchor->GetName(tmpText)) && !tmpText.IsEmpty())
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
  return nsEditor::NodeIsType(node, nsEditProperty::div);
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
    if (attrVal.EqualsLiteral("cite"))
      return PR_TRUE;
  }

  // ... but our plaintext mailcites by "_moz_quote=true".  go figure.
  attrName.AssignLiteral("_moz_quote");
  res = elem->GetAttribute(attrName, attrVal);
  if (NS_SUCCEEDED(res))
  {
    ToLowerCase(attrVal);
    if (attrVal.EqualsLiteral("true"))
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
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(node);
  return (nodeAtom == nsEditProperty::textarea)
      || (nodeAtom == nsEditProperty::select)
      || (nodeAtom == nsEditProperty::button)
      || (nodeAtom == nsEditProperty::input);
}

PRBool
nsHTMLEditUtils::SupportsAlignAttr(nsIDOMNode * aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditUtils::SupportsAlignAttr");
  nsCOMPtr<nsIAtom> nodeAtom = nsEditor::GetTag(aNode);
  return (nodeAtom == nsEditProperty::hr)
      || (nodeAtom == nsEditProperty::table)
      || (nodeAtom == nsEditProperty::tbody)
      || (nodeAtom == nsEditProperty::tfoot)
      || (nodeAtom == nsEditProperty::thead)
      || (nodeAtom == nsEditProperty::tr)
      || (nodeAtom == nsEditProperty::td)
      || (nodeAtom == nsEditProperty::th)
      || (nodeAtom == nsEditProperty::div)
      || (nodeAtom == nsEditProperty::p)
      || (nodeAtom == nsEditProperty::h1)
      || (nodeAtom == nsEditProperty::h2)
      || (nodeAtom == nsEditProperty::h3)
      || (nodeAtom == nsEditProperty::h4)
      || (nodeAtom == nsEditProperty::h5)
      || (nodeAtom == nsEditProperty::h6);
}
