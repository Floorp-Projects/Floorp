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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#include "nsP3PUtils.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsReadableUtils.h"

nsresult
nsP3PUtils::GetAttributeValue(nsIDOMNode* aNode, 
                              char* aAttrName, 
                              nsAString& aAttrValue) 
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aAttrName);

  aAttrValue.Truncate();

  nsCOMPtr<nsIDOMNamedNodeMap> attributeNodes;
  aNode->GetAttributes(getter_AddRefs(attributeNodes));
  NS_ENSURE_TRUE(attributeNodes, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMNode> attributeNode;
  nsCOMPtr<nsIDOMAttr> domAttr;
  PRUint32             attrCount;
  nsAutoString         name;

  attributeNodes->GetLength(&attrCount);
  
  PRUint32 i;
  for (i = 0; i < attrCount; i++) {
    attributeNodes->Item(i, getter_AddRefs(attributeNode));
    NS_ENSURE_TRUE(attributeNode, NS_ERROR_UNEXPECTED);
         
    attributeNode->GetLocalName(name);
    if (!name.IsEmpty() && name.EqualsIgnoreCase(aAttrName)) {
      domAttr = do_QueryInterface(attributeNode);
      NS_ENSURE_TRUE(domAttr, NS_ERROR_UNEXPECTED);
      return domAttr->GetValue(aAttrValue);
    }
  }

  return NS_OK;
}

static PRBool
IsCharInSet(const char* aSet,
            const PRUnichar aChar)
{
  PRUnichar ch;
  while ((ch = *aSet)) {
    if (aChar == PRUnichar(ch)) {
      return PR_TRUE;
    }
    ++aSet;
  }
  return PR_FALSE;
}

/**
 * This method strips leading/trailing chars, in given set, from string.
 */
const nsDependentSubstring
nsP3PUtils::TrimCharsInSet(const char* aSet,
                           const nsAString& aValue)
{
  nsAString::const_iterator valueCurrent, valueEnd;

  aValue.BeginReading(valueCurrent);
  aValue.EndReading(valueEnd);

  // Skip charaters in the beginning
  while (valueCurrent != valueEnd) {
    if (!IsCharInSet(aSet, *valueCurrent)) {
      break;
    }
    ++valueCurrent;
  }

  if (valueCurrent != valueEnd) {
    for (;;) {
      --valueEnd;
      if (!IsCharInSet(aSet, *valueEnd)) {
        break;
      }
    }
    ++valueEnd; // Step beyond the last character we want in the value.
  }

  // valueEnd should point to the char after the last to copy
  return Substring(valueCurrent, valueEnd);
}

/** 
 *  This routine checks to see if the uri, specified with INCLUDE, EXCLUDE,
 *  EMBEDDED-INCLUDE, EMBEDDED-EXCLUDE, includes the current path. 
 *  Note: The '*' character, that is used in the policy reference file, 
 *  is treated as a wildcard ( zero or more characters ).
 *  
 *  This method compares two strings where the lhs string value may contain 
 *  asterisk. Therefore, an lhs with a value of "he*o" should be equal to a rhs
 *  value of "hello" or "hero" etc.. These strings are compared as follows:
 *  1) Characters before the first asterisk are compared from left to right.
 *     Thus if the lhs string did not contain an asterisk then we just do
 *     a simple string comparison.
 *  2) Match a pattern, found between asterisk. That is, if lhs and rhs were 
 *     "h*ll*" and "hello" respectively, then compare the pattern "ll".
 *  3) Characters after the last asterisk are compared from right to left.
 *     Thus, "*lo" == "hello" and != "blow"
 */
PRBool 
nsP3PUtils::IsPathIncluded(const nsAString& aLhs, 
                           const nsAString& aRhs) 
{
  nsAString::const_iterator lhs_begin, lhs_end;
  nsAString::const_iterator rhs_begin, rhs_end;
 
  aLhs.BeginReading(lhs_begin);
  aLhs.EndReading(lhs_end);
  aRhs.BeginReading(rhs_begin);
  aRhs.EndReading(rhs_end);

  nsAutoString pattern;
  PRBool pattern_before_asterisk = PR_TRUE; 
  nsAString::const_iterator curr_posn = lhs_begin;
  while (curr_posn != lhs_end) {
    if (*lhs_begin == '*') {
      pattern_before_asterisk = PR_FALSE;
      ++lhs_begin; // Do this to not include '*' when pattern matching.
    }
    else if (pattern_before_asterisk) {
      // Match character by character to see if lhs and rhs are identical
      if (*curr_posn != *rhs_begin) {
        return PR_FALSE;
      }
      ++lhs_begin;
      ++curr_posn;
      ++rhs_begin;
      if (rhs_begin == rhs_end &&
          curr_posn == lhs_end) {
        return PR_TRUE; // lhs and rhs matched perfectly
      }
    }
    else if (++curr_posn == lhs_end) {
      if (curr_posn != lhs_begin) {
        // Here we're matching the last few characters to make sure
        // that lhs is actually equal to rhs. Ex. "a*c" != "abcd"
        PRBool done;
        for (;;) {
          --curr_posn;
          done = (--rhs_end == rhs_begin) || (curr_posn == lhs_begin);
          if (*rhs_end != *curr_posn) {
            return PR_FALSE;
          }
          if (done) {
            return PR_TRUE;
          }
        }
      }
      // No discrepency between lhs and rhs
      return PR_TRUE;
    }
    else if (*curr_posn == '*') {
      // Matching pattern between asterisks. That is, in "h*ll*" we
      // check to see if "ll" exists in the rhs string.
      nsAString::const_iterator tmp_end = rhs_end;
      CopyUnicodeTo(lhs_begin, curr_posn, pattern);
      if (!FindInReadable(pattern, rhs_begin, rhs_end)) 
         return PR_FALSE;
      rhs_begin = rhs_end;
      rhs_end   = tmp_end;
      lhs_begin = curr_posn;
    }
  }

  return PR_FALSE;
}

nsresult
nsP3PUtils::DeterminePolicyScope(const nsVoidArray& aNodeList, 
                                 const char* aPath, 
                                 PRBool* aOut)
{

  NS_ENSURE_ARG_POINTER(aPath);
  NS_ENSURE_ARG_POINTER(aOut);

  *aOut = PR_FALSE;
  PRInt32 count = aNodeList.Count();

  nsAutoString value;
  nsCOMPtr<nsIDOMNode> node, child;
  PRInt32 i;
  for (i = 0; i < count && !(*aOut); i++) {
    nsIDOMNode* node = NS_REINTERPRET_CAST(nsIDOMNode*, aNodeList.ElementAt(i));
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

    node->GetFirstChild(getter_AddRefs(child));
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);

    child->GetNodeValue(value);
    static const char* kWhitespace = " \n\r\t\b";
    value = TrimCharsInSet(kWhitespace, value);
    *aOut = IsPathIncluded(value, NS_ConvertUTF8toUCS2(aPath));
  }

  return NS_OK;
}

/**
 * Note: Boris Zbarsky points out that this method,
 * GetElementsByTagName, is _NOT_ live, unlike the DOM one.
 */
nsresult 
nsP3PUtils::GetElementsByTagName(nsIDOMNode* aNode, 
                                 const nsAString& aTagName, 
                                 nsVoidArray& aReturn)
{
  NS_ENSURE_ARG_POINTER(aNode);
    
  CleanArray(aReturn);
  
  nsCOMPtr<nsIDOMNodeList> children;
  aNode->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_TRUE(children, NS_ERROR_UNEXPECTED);

  PRUint32 count;
  children->GetLength(&count);

  nsAutoString name;
  nsIDOMNode* node;
  PRUint32 i;
  for (i = 0; i < count; i++) {
    children->Item(i, &node);
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);
    
    PRUint16 type;
    node->GetNodeType(&type);

    if (type == nsIDOMNode::ELEMENT_NODE) {
      node->GetNodeName(name);
      if (aTagName.Equals(name)) {
        NS_IF_ADDREF(node);
        aReturn.AppendElement((void*)node);
      }
    }
  }

  return NS_OK;
}

void
nsP3PUtils::CleanArray(nsVoidArray& aArray)
{
  PRInt32 count = aArray.Count();
  nsCOMPtr<nsIDOMNode> node;
  while (count) {
    nsIDOMNode* node =
      NS_REINTERPRET_CAST(nsIDOMNode*, aArray.ElementAt(--count));
    aArray.RemoveElementAt(count);
    NS_IF_RELEASE(node);
  }
}
