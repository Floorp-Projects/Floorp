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
#include "nsEditor.h"
#include "nsIDOMNode.h"
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
  tag.ToLowerCase();
  if ( (tag.EqualsWithConversion("h1")) ||
       (tag.EqualsWithConversion("h2")) ||
       (tag.EqualsWithConversion("h3")) ||
       (tag.EqualsWithConversion("h4")) ||
       (tag.EqualsWithConversion("h5")) ||
       (tag.EqualsWithConversion("h6")) )
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
// IsListItem: true if node an html list item
//                  
PRBool 
nsHTMLEditUtils::IsListItem(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsListItem");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("li") ||
      tag.EqualsWithConversion("dd") ||
      tag.EqualsWithConversion("dt"))
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
  if (tagName.EqualsWithConversion("table") || tagName.EqualsWithConversion("tr") || 
      tagName.EqualsWithConversion("td")    || tagName.EqualsWithConversion("th") ||
      tagName.EqualsWithConversion("thead") || tagName.EqualsWithConversion("tfoot") ||
      tagName.EqualsWithConversion("tbody") || tagName.EqualsWithConversion("caption"))
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
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("td") || tag.EqualsWithConversion("th"))
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
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("td") || 
      tag.EqualsWithConversion("th") ||
      tag.EqualsWithConversion("caption") )
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
  tag.ToLowerCase();
  if ( (tag.EqualsWithConversion("dl")) ||
       (tag.EqualsWithConversion("ol")) ||
       (tag.EqualsWithConversion("ul")) )
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
// IsDefinitionList: true if node an html definition list
//                  
PRBool 
nsHTMLEditUtils::IsDefinitionList(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("dl"));
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
// IsAnchor: true if node an html anchor node
//                  
PRBool 
nsHTMLEditUtils::IsAnchor(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("a"));
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
// IsNormalDiv: true if node an html div node, without type = _moz
//                  
PRBool 
nsHTMLEditUtils::IsNormalDiv(nsIDOMNode *node)
{
  if (IsDiv(node) && !nsTextEditUtils::HasMozAttr(node)) return PR_TRUE;
  return PR_FALSE;
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
  if (IsBlockquote(node))
  {
    nsCOMPtr<nsIDOMElement> bqElem = do_QueryInterface(node);
    nsAutoString typeAttrName; typeAttrName.AssignWithConversion("type");
    nsAutoString typeAttrVal;
    nsresult res = bqElem->GetAttribute(typeAttrName, typeAttrVal);
    typeAttrVal.ToLowerCase();
    if (NS_SUCCEEDED(res))
    {
      if (typeAttrVal.EqualsWithConversion("cite", PR_TRUE, 4))
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsTextarea: true if node is an html textarea node
//                  
PRBool 
nsHTMLEditUtils::IsTextarea(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("textarea"));
}


///////////////////////////////////////////////////////////////////////////
// IsMap: true if node is an html map node
//                  
PRBool 
nsHTMLEditUtils::IsMap(nsIDOMNode *node)
{
  return nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("map"));
}


PRBool 
nsHTMLEditUtils::IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent) 
{
  if (!aNode && !aParent) return PR_FALSE;
  if (aNode == aParent) return PR_FALSE;
  
  nsCOMPtr<nsIDOMNode> parent, node = do_QueryInterface(aNode);
  nsresult res;
  
  do
  {
    res = node->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return PR_FALSE;
    if (parent.get() == aParent) return PR_TRUE;
    node = parent;
  } while (parent);
  
  return PR_FALSE;
}


PRBool 
nsHTMLEditUtils::IsLeafNode(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  PRBool hasChildren = PR_FALSE;
  aNode->HasChildNodes(&hasChildren);
  return !hasChildren;
}

PRBool
nsHTMLEditUtils::SupportsAlignAttr(nsIDOMNode * aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditUtils::SupportsAlignAttr");
  nsAutoString tag;
  nsEditor::GetTagString(aNode, tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("hr") ||
      tag.EqualsWithConversion("table") ||
      tag.EqualsWithConversion("tbody") ||
      tag.EqualsWithConversion("tfoot") ||
      tag.EqualsWithConversion("thead") ||
      tag.EqualsWithConversion("tr") ||
      tag.EqualsWithConversion("td") ||
      tag.EqualsWithConversion("th") ||
      tag.EqualsWithConversion("div") ||
      tag.EqualsWithConversion("p") ||
      tag.EqualsWithConversion("h1") ||
      tag.EqualsWithConversion("h2") ||
      tag.EqualsWithConversion("h3") ||
      tag.EqualsWithConversion("h4") ||
      tag.EqualsWithConversion("h5") ||
      tag.EqualsWithConversion("h6")) {
    return PR_TRUE;
  }
  return PR_FALSE;
}
