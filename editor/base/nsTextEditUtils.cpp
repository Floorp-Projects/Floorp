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

#include "nsTextEditUtils.h"

#include "nsEditor.h"
#include "nsPlaintextEditor.h"

#include "nsString.h"

/********************************************************
 *  helper methods from nsTextEditRules
 ********************************************************/
 
PRBool
nsTextEditUtils::NodeIsType(nsIDOMNode *aNode, const nsAReadableString& aTag)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditUtils::NodeIsType");
  if (aNode)
  {
    nsAutoString tag;
    nsEditor::GetTagString(aNode,tag);
    tag.ToLowerCase();
    if (tag.Equals(aTag))
      return PR_TRUE;
  }
  return PR_FALSE;
}

/********************************************************
 *  helper methods from nsTextEditRules
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// IsBody: true if node an html body node
//                  
PRBool 
nsTextEditUtils::IsBody(nsIDOMNode *node)
{
  return NodeIsType(node, NS_LITERAL_STRING("body"));
}


///////////////////////////////////////////////////////////////////////////
// IsBreak: true if node an html break node
//                  
PRBool 
nsTextEditUtils::IsBreak(nsIDOMNode *node)
{
  return NodeIsType(node, NS_LITERAL_STRING("br"));
}


///////////////////////////////////////////////////////////////////////////
// IsMozBR: true if node an html br node with type = _moz
//                  
PRBool 
nsTextEditUtils::IsMozBR(nsIDOMNode *node)
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
nsTextEditUtils::HasMozAttr(nsIDOMNode *node)
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
nsTextEditUtils::InBody(nsIDOMNode *node, nsIEditor *editor)
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

///////////////////////////////////////////////////////////////////////////
// nsAutoEditInitRulesTrigger methods
//
nsAutoEditInitRulesTrigger::nsAutoEditInitRulesTrigger( nsPlaintextEditor *aEd, nsresult &aRes) : mEd(aEd), mRes(aRes)
{
    if (mEd) mEd->BeginEditorInit();
}

nsAutoEditInitRulesTrigger::~nsAutoEditInitRulesTrigger()
{
    if (mEd) mRes = mEd->EndEditorInit();
}

