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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  David Landwehr <dlandwehr@novell.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXFormsMDG.h"

#include "nsVoidArray.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsXFormsXPathNode.h"

MOZ_DECL_CTOR_COUNTER(nsXFormsMDG)

nsXFormsMDG::nsXFormsMDG()
  : mEngine(this)
{
  MOZ_COUNT_CTOR(nsXFormsMDG);
}

nsXFormsMDG::~nsXFormsMDG()
{
  MOZ_COUNT_DTOR(nsXFormsMDG);
}

nsresult
nsXFormsMDG::Init()
{
  return mEngine.Init();
}

nsresult
nsXFormsMDG::CreateNewChild(nsIDOMNode* aContextNode, const nsAString& aNodeValue,
                            nsIDOMNode* aBeforeNode)
{
  nsresult rv;

  nsCOMPtr<nsIDOMDocument> document;
  rv = aContextNode->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMText> textNode;
  rv = document->CreateTextNode(aNodeValue, getter_AddRefs(textNode));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMNode> newNode;
  if (aBeforeNode) {
    rv = aContextNode->InsertBefore(textNode, aBeforeNode, getter_AddRefs(newNode));
  } else {
    rv = aContextNode->AppendChild(textNode, getter_AddRefs(newNode));
  }

  return rv;
}

nsresult
nsXFormsMDG::AddMIP(ModelItemPropName aType, nsIDOMXPathExpression* aExpression,
                    nsXFormsMDGSet* aDeps, PRBool aDynFunc, nsIDOMNode* aContextNode,
                    PRInt32 aContextPos, PRInt32 aContextSize)
{
  NS_ENSURE_ARG(aExpression);
  NS_ENSURE_ARG(aContextNode);

  return mEngine.Insert(aContextNode, aExpression, aDeps, aDynFunc, aContextPos, aContextSize, aType);
}

nsresult
nsXFormsMDG::MarkNodeAsChanged(nsIDOMNode* aContextNode)
{
  return mEngine.MarkNode(aContextNode);
}

nsresult
nsXFormsMDG::Recalculate(nsXFormsMDGSet& aChangedNodes)
{
  return mEngine.Calculate(aChangedNodes);
}

nsresult
nsXFormsMDG::Rebuild()
{
  return mEngine.Rebuild();
}

void
nsXFormsMDG::ClearDispatchFlags()
{
  mEngine.ClearDispatchFlags();
}

void
nsXFormsMDG::Clear() {
  mEngine.Clear();
}

nsresult
nsXFormsMDG::GetNodeValue(nsIDOMNode* aContextNode, nsAString& aNodeValue)
{
  nsresult rv;
  nsCOMPtr<nsIDOMNode> childNode;

  PRUint16 nodeType;
  rv = aContextNode->GetNodeType(&nodeType);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::CDATA_SECTION_NODE:
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
  case nsIDOMNode::COMMENT_NODE:
    rv = aContextNode->GetNodeValue(aNodeValue);
    NS_ENSURE_SUCCESS(rv, rv);
    break;

  case nsIDOMNode::ELEMENT_NODE:
    rv = aContextNode->GetFirstChild(getter_AddRefs(childNode));
    if (NS_FAILED(rv) || !childNode) {
      // No child
      aNodeValue.Truncate(0);
    } else {
      PRUint16 childType;
      rv = childNode->GetNodeType(&childType);
      NS_ENSURE_SUCCESS(rv, rv);
  
      if (   childType == nsIDOMNode::TEXT_NODE
          || childType == nsIDOMNode::CDATA_SECTION_NODE) {
        rv = childNode->GetNodeValue(aNodeValue);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Not a text child
        aNodeValue.Truncate(0);
      }
    }
    break;
          
  default:
    // Asked for a node which cannot have a text child
    // TODO: Should return more specific error?
    return NS_ERROR_ILLEGAL_VALUE;
    break;
  }
  
  return NS_OK;
}

nsresult
nsXFormsMDG::SetNodeValue(nsIDOMNode* aContextNode, nsAString& aNodeValue, PRBool aMarkNode)
{
  if (IsReadonly(aContextNode)) {
    // TODO: Better feedback for readonly nodes?
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIDOMNode> childNode;
  PRUint16 nodeType;
  rv = aContextNode->GetNodeType(&nodeType);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::CDATA_SECTION_NODE:
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
  case nsIDOMNode::COMMENT_NODE:
    // TODO: Check existing value, and ignore if same??
    rv = aContextNode->SetNodeValue(aNodeValue);
    NS_ENSURE_SUCCESS(rv, rv);

    break;

  case nsIDOMNode::ELEMENT_NODE:
    rv = aContextNode->GetFirstChild(getter_AddRefs(childNode));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!childNode) {
      rv = CreateNewChild(aContextNode, aNodeValue);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      PRUint16 childType;
      rv = childNode->GetNodeType(&childType);
      NS_ENSURE_SUCCESS(rv, rv);
      
      if (   childType == nsIDOMNode::TEXT_NODE
          || childType == nsIDOMNode::CDATA_SECTION_NODE) {
        rv = childNode->SetNodeValue(aNodeValue);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Not a text child, create a new one
        rv = CreateNewChild(aContextNode, aNodeValue, childNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    break;
          
  default:
    // Unsupported nodeType
    // TODO: Should return more specific error?
    return NS_ERROR_ILLEGAL_VALUE;
    break;
  }
  
  // NB: Never reached for Readonly nodes.  
  if (aMarkNode) {
      MarkNodeAsChanged(aContextNode);
  }
  
  return NS_OK;
}


/**********************************************/
/*             Bit test functions             */
/**********************************************/
PRBool
nsXFormsMDG::IsValid(nsIDOMNode* aContextNode)
{
  
  PRBool valid = mEngine.Test(aContextNode, MDG_FLAG_CONSTRAINT);
  if (valid) {
    // TODO: needs Schema support
    // valid = mSchemaHandler->ValidateNode(aContextNode);
  }
  return valid;
}

PRBool
nsXFormsMDG::IsConstraint(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_CONSTRAINT);
}

PRBool
nsXFormsMDG::ShouldDispatchValid(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_DISPATCH_VALID_CHANGED);
}

PRBool
nsXFormsMDG::IsReadonly(nsIDOMNode* aContextNode)
{
  PRBool valid;
  if (   mEngine.Test(aContextNode, MDG_FLAG_READONLY)
      || mEngine.Test(aContextNode, MDG_FLAG_INHERITED_READONLY)) {
    valid = PR_TRUE;
  } else {
    valid = PR_FALSE;
  }
  return valid;
}

PRBool
nsXFormsMDG::ShouldDispatchReadonly(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_DISPATCH_READONLY_CHANGED);
}


PRBool
nsXFormsMDG::IsRelevant(nsIDOMNode* aContextNode)
{
  PRBool valid;
  if (   mEngine.Test(aContextNode, MDG_FLAG_RELEVANT)
      && mEngine.Test(aContextNode, MDG_FLAG_INHERITED_RELEVANT)) {
    valid = PR_TRUE;
  } else {
    valid = PR_FALSE;
  }
  return valid;
}

PRBool
nsXFormsMDG::ShouldDispatchRelevant(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_DISPATCH_RELEVANT_CHANGED);
}

PRBool
nsXFormsMDG::IsRequired(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_REQUIRED);
}

PRBool
nsXFormsMDG::ShouldDispatchRequired(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_DISPATCH_REQUIRED_CHANGED);
}

PRBool
nsXFormsMDG::ShouldDispatchValueChanged(nsIDOMNode* aContextNode)
{
  return mEngine.Test(aContextNode, MDG_FLAG_DISPATCH_VALUE_CHANGED);
}
