/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

