/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsHTMLEditUtils.h"

#include "nsString.h"
#include "nsEditor.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"

/********************************************************
 *  helper methods from nsTextEditRules
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// IsBody: true if node an html body node
//                  
PRBool 
nsHTMLEditUtils::IsBody(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsBody");
  if (node)
  {
    nsAutoString tag;
    nsEditor::GetTagString(node,tag);
    if (tag.EqualsWithConversion("body"))
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}



///////////////////////////////////////////////////////////////////////////
// IsBreak: true if node an html break node
//                  
PRBool 
nsHTMLEditUtils::IsBreak(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsBreak");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("br"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsBreak: true if node an html break node
//                  
PRBool 
nsHTMLEditUtils::IsBig(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsBig");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("big"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsBreak: true if node an html break node
//                  
PRBool 
nsHTMLEditUtils::IsSmall(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsSmall");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("small"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsMozBR: true if node an html br node with type = _moz
//                  
PRBool 
nsHTMLEditUtils::IsMozBR(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsMozBR");
  if (IsBreak(node) && HasMozAttr(node)) return PR_TRUE;
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// HasMozAttr: true if node has type attribute = _moz
//             (used to indicate the div's and br's we use in
//              mail compose rules)
//                  
PRBool 
nsHTMLEditUtils::HasMozAttr(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::HasMozAttr");
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(node);
  if (elem)
  {
    nsAutoString typeAttrName; typeAttrName.AssignWithConversion("type");
    nsAutoString typeAttrVal;
    nsresult res = elem->GetAttribute(typeAttrName, typeAttrVal);
    typeAttrVal.ToLowerCase();
    if (NS_SUCCEEDED(res) && (typeAttrVal.EqualsWithConversion("_moz")))
      return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// InBody: true if node is a descendant of the body
//                  
PRBool 
nsHTMLEditUtils::InBody(nsIDOMNode *node, nsIEditor *editor)
{
  if ( node )
  {
    nsCOMPtr<nsIDOMElement> bodyElement;
    nsresult res = editor->GetRootElement(getter_AddRefs(bodyElement));
    if (NS_FAILED(res) || !bodyElement)
      return res?res:NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);
    nsCOMPtr<nsIDOMNode> tmp;
    nsCOMPtr<nsIDOMNode> p = node;
    while (p && p!= bodyNode)
    {
      if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp)
        return PR_FALSE;
      p = tmp;
    }
  }
  return PR_TRUE;
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
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsParagraph");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("p"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
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
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTable");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag.EqualsWithConversion("table"))
    return PR_TRUE;

  return PR_FALSE;
}

///////////////////////////////////////////////////////////////////////////
// IsTableRow: true if node an html tr
//                  
PRBool 
nsHTMLEditUtils::IsTableRow(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsTableRow");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("tr"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
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
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsOrderedList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("ol"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsUnorderedList: true if node an html unordered list
//                  
PRBool 
nsHTMLEditUtils::IsUnorderedList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsUnorderedList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("ul"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsDefinitionList: true if node an html definition list
//                  
PRBool 
nsHTMLEditUtils::IsDefinitionList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsDefinitionList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("dl"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsBlockquote: true if node an html blockquote node
//                  
PRBool 
nsHTMLEditUtils::IsBlockquote(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsBlockquote");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("blockquote"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsPre: true if node an html pre node
//                  
PRBool 
nsHTMLEditUtils::IsPre(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsPre");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("pre"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsAddress: true if node an html address node
//                  
PRBool 
nsHTMLEditUtils::IsAddress(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsAddress");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("address"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsAnchor: true if node an html anchor node
//                  
PRBool 
nsHTMLEditUtils::IsAnchor(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsAnchor");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("a"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsImage: true if node an html image node
//                  
PRBool 
nsHTMLEditUtils::IsImage(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsImage");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("img"))
  {
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
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::IsDiv");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("div"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsNormalDiv: true if node an html div node, without type = _moz
//                  
PRBool 
nsHTMLEditUtils::IsNormalDiv(nsIDOMNode *node)
{
  if (IsDiv(node) && !nsHTMLEditUtils::HasMozAttr(node)) return PR_TRUE;
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsMozDiv: true if node an html div node with type = _moz
//                  
PRBool 
nsHTMLEditUtils::IsMozDiv(nsIDOMNode *node)
{
  if (IsDiv(node) && HasMozAttr(node)) return PR_TRUE;
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



