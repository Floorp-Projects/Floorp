/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Heikki Toivonen <heikki@netscape.com> (original author)
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

/**
 * Implementation for FIXptr, a W3C proposal.
 *
 * http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
 *
 */
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsFIXptr.h"
#include "nsCRT.h"

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

// Get nth child element
static nsresult
GetChild(nsIDOMNode *aParent, PRInt32 aChildNum, nsIDOMNode **aChild)
{
  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aChild);

  *aChild = nsnull;
  nsCOMPtr<nsIDOMNodeList> list;
  aParent->GetChildNodes(getter_AddRefs(list));
  if (!list)
    return NS_OK;
  
  PRUint32 count;
  list->GetLength(&count);
  PRUint32 i;
  PRInt32 curChildNum = 0;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> node;
    list->Item(i, getter_AddRefs(node));
    if (!node)
      break;
    PRUint16 nodeType;
    node->GetNodeType(&nodeType);
    if (nodeType == nsIDOMNode::ELEMENT_NODE) {
      curChildNum++;
    }
    if (curChildNum == aChildNum) {
      *aChild = node;
      NS_ADDREF(*aChild);
      break;
    }
  }
  return NS_OK;
}

// Get range for char index
static nsresult
GetCharRange(nsIDOMNode *aParent, PRInt32 aCharNum, nsIDOMRange **aRange)
{
  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aRange);

  *aRange = nsnull;
  nsCOMPtr<nsIDOMNodeList> list;
  aParent->GetChildNodes(getter_AddRefs(list));
  if (!list)
    return NS_OK;

  PRUint32 count;
  list->GetLength(&count);
  PRUint32 i;
  PRInt32 maxCharNum = 0;
  PRInt32 prevCharNum = 0;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> node;
    list->Item(i, getter_AddRefs(node));
    if (!node)
      break;
    PRUint16 nodeType;
    node->GetNodeType(&nodeType);
    if (nodeType & (nsIDOMNode::TEXT_NODE | nsIDOMNode::CDATA_SECTION_NODE)) {
      nsAutoString value;
      node->GetNodeValue(value);
      maxCharNum += value.Length();
    }
    if (maxCharNum >= aCharNum) {
      nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
      if (!range)
        return NS_ERROR_OUT_OF_MEMORY;
      range->SetStart(node, aCharNum - prevCharNum);
      range->SetEnd(node, aCharNum - prevCharNum + 1);
      *aRange = range;
      NS_ADDREF(*aRange);
      break;
    }
    prevCharNum = maxCharNum;
  }
  return NS_OK;
}

// Get node by tumbler
static nsresult
GetTumblerNode(nsIDOMNode *aParent, const nsString &aTumbler,
               nsIDOMNode **aNode)
{
  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aNode);

  *aNode = nsnull;
  nsAutoString tumbler(aTumbler);
  if (!tumbler.IsEmpty() && tumbler[0] == '/')
    tumbler.Cut(0, 1);

  nsCOMPtr<nsIDOMNode> node(aParent);
  while (!tumbler.IsEmpty() && node) {
    PRInt32 sep = tumbler.FindChar('/');
    if (sep > 0) {
      nsAutoString num;
      tumbler.Left(num, sep);
      PRInt32 error;
      PRInt32 n = num.ToInteger(&error);
      if (n <= 0) {
        node = nsnull;
        break;
      }
      nsCOMPtr<nsIDOMNode> child;
      GetChild(node, n, getter_AddRefs(child));
      node = child;
    } else {
      // Last
      PRInt32 error;
      PRInt32 n = tumbler.ToInteger(&error);
      if (n <= 0) {
        node = nsnull;
        break;
      }
      nsCOMPtr<nsIDOMNode> child;
      GetChild(node, n, getter_AddRefs(child));
      node = child;
      break;
    }
    tumbler.Cut(0, sep + 1);
  }
  *aNode = node;
  NS_IF_ADDREF(*aNode);
  return NS_OK;
}

static nsresult
GetRange(nsIDOMDocument *aDocument, const nsAString& aExpression,
         nsIDOMRange **aRange)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMNode> node;
  if (nsCRT::IsAsciiAlpha(aExpression.First())) {
    // name
    nsAutoString id;
    const nsAutoString expression(aExpression);
    PRInt32 sep = expression.FindCharInSet("/(");
    if (sep > 0) {
      expression.Left(id,sep);
      nsCOMPtr<nsIDOMElement> element;
      aDocument->GetElementById(id, getter_AddRefs(element));
      node = do_QueryInterface(element);
      if (node) {
        if (expression[sep] == '/') {
          // tumbler
          nsAutoString tumbler;
          expression.Mid(tumbler, sep, expression.Length());
          sep = tumbler.FindChar('(');
          if (sep > 0)
            tumbler.Truncate(sep);
          nsCOMPtr<nsIDOMNode> child;
          GetTumblerNode(node, tumbler, getter_AddRefs(child));
          node = child;
        }
        // char
        sep = expression.FindChar('(');
        if (sep > 0) {
          nsAutoString charNum(aExpression);
          if (charNum[charNum.Length() -1] == ')') {
            charNum.Truncate(charNum.Length() - 1);
            charNum.Cut(0, sep + 1);
            PRInt32 error;
            PRInt32 n = charNum.ToInteger(&error);
            if (n < 1)
              return NS_OK;
            rv = GetCharRange(node, n - 1, aRange);
            if (!*aRange) {
              node = nsnull;
            }
          }
        }
      }
    } else {
      // just name
      nsCOMPtr<nsIDOMElement> element;
      aDocument->GetElementById(expression, getter_AddRefs(element));
      node = do_QueryInterface(element);
    }
  } else if (aExpression.First() == '/') {
    // Starts with tumbler
    node = do_QueryInterface(aDocument);
    nsCOMPtr<nsIDOMNode> child;
    nsAutoString tumbler(aExpression);
    PRInt32 sep = tumbler.FindChar('(');
    if (sep > 0)
      tumbler.Truncate(sep);
    GetTumblerNode(node, tumbler, getter_AddRefs(child));
    node = child;
    // char
    sep = aExpression.FindChar('(');
    if (sep > 0) {
      nsAutoString charNum(aExpression);
      if (charNum[charNum.Length() -1] == ')') {
        charNum.Truncate(charNum.Length() - 1);
        charNum.Cut(0, sep + 1);
        PRInt32 error;
        PRInt32 n = charNum.ToInteger(&error);
        if (n < 1)
          return NS_OK;
        rv = GetCharRange(node, n - 1, aRange);
        if (!*aRange) {
          node = nsnull;
        }
      }
    }
  } else {
    // Not FIXptr
  }

  if (NS_SUCCEEDED(rv) && !*aRange && node) {
    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));  
    if (!range)
      return NS_ERROR_OUT_OF_MEMORY;
    range->SelectNode(node);
    *aRange = range;
    NS_ADDREF(*aRange);
  }

  return rv;
}

/**
 * Evaluate a FIXptr expression.
 *
 * @param aDocument   The document in which to evaluate.
 * @param aExpression The FIXptr expression string to evaluate.
 * @param aRange      The resulting range.
 */
nsresult 
nsFIXptr::Evaluate(nsIDOMDocument *aDocument,
                   const nsAString& aExpression,
                   nsIDOMRange **aRange)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nsnull;

  nsresult rv;

  PRInt32 split = aExpression.FindChar(',');
  if (split >= 0) {
    nsCOMPtr<nsIDOMRange> range1, range2;

    const nsDependentSubstring &expr1 = Substring(aExpression, 0, split);
    const nsDependentSubstring &expr2 =
      Substring(aExpression, split + 1, aExpression.Length() - (split + 1));

    rv = GetRange(aDocument, expr1, getter_AddRefs(range1)); 
    if (!range1)
      return rv;

    rv = GetRange(aDocument, expr2, getter_AddRefs(range2));
    if (!range2)
      return rv;

    nsCOMPtr<nsIDOMNode> begin, end;
    PRInt32 beginOffset, endOffset;
    range1->GetStartContainer(getter_AddRefs(begin));
    range1->GetStartOffset(&beginOffset);
    range2->GetEndContainer(getter_AddRefs(end));
    range2->GetEndOffset(&endOffset);

    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID, &rv));
    if (NS_FAILED(rv))
      return rv;

    range->SetStart(begin, beginOffset);
    range->SetEnd(end, endOffset);
    *aRange = range;
    NS_ADDREF(*aRange);
  } else {
    rv = GetRange(aDocument, aExpression, aRange); 
  }

  return rv;
}
