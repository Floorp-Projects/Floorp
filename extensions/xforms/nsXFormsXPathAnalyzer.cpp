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

#include "nsXFormsXPathAnalyzer.h"
#include "nsIDOMXPathResult.h"

#ifdef DEBUG
// #define DEBUG_XF_ANALYZER
#endif

MOZ_DECL_CTOR_COUNTER(nsXFormsXPathAnalyzer)

nsXFormsXPathAnalyzer::nsXFormsXPathAnalyzer(nsIDOMXPathEvaluator* aEvaluator, nsIDOMXPathNSResolver* aResolver)
  : mEvaluator(aEvaluator), mResolver(aResolver)
{
  MOZ_COUNT_CTOR(nsXFormsXPathAnalyzer);
}

nsXFormsXPathAnalyzer::~nsXFormsXPathAnalyzer()
{
  MOZ_COUNT_DTOR(nsXFormsXPathAnalyzer);
}

nsresult
nsXFormsXPathAnalyzer::Analyze(nsIDOMNode* aContextNode, const nsXFormsXPathNode* aNode,
                               nsIDOMXPathExpression* aExpression, const nsAString* aExprString,
                               nsXFormsMDGSet* aSet)
{
  NS_ENSURE_TRUE(aContextNode, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aNode, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aExpression, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aExprString, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aSet, NS_ERROR_INVALID_ARG);

  mCurExpression = aExpression;
  mCurExprString = aExprString;
  mCurSet = aSet;

#ifdef DEBUG_XF_ANALYZER
  printf("=====================================\n");
  printf("Analyzing: %s\n", NS_ConvertUCS2toUTF8(*mCurExprString).get());
  printf("=====================================\n");
#endif

  nsresult rv = AnalyzeRecursively(aContextNode, aNode->mChild, 0);
#ifdef DEBUG_XF_ANALYZER
  printf("-------------------------------------\n");
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsXFormsXPathAnalyzer::AnalyzeRecursively(nsIDOMNode* aContextNode, const nsXFormsXPathNode* aNode,
                                          PRUint32 aIndent)
{
  nsXFormsXPathNode* t;
  nsAutoString xp;
  nsresult rv;
  nsCOMPtr<nsIDOMXPathResult> result;
  nsCOMPtr<nsIDOMNode> node;
  char strbuf[100];
  char* strpos;

#ifdef DEBUG_beaufour_xxx
  printf("nsXFormsXPathParser::AnalyzeRecursively(%p)\n", (void*) aNode);
#endif
  
  for (; aNode; aNode = aNode->mSibling) {
#ifdef DEBUG_beaufour_xxx
    printf("\tChild: %p, Sibling: %p\n", (void*) aNode->mChild, (void*) aNode->mSibling);
    printf("\tIndex: %d - %d\n", aNode->mStartIndex, aNode->mEndIndex);
    printf("\tCon: %d, Predicate: %d, Literal: %d\n", aNode->mCon, aNode->mPredicate, aNode->mLiteral);
#endif

    if (   aNode->mEndIndex < 0
        || aNode->mStartIndex >= aNode->mEndIndex
        || ((PRUint32) aNode->mEndIndex == mCurExprString->Length() && aNode->mStartIndex == 0)) { 
      continue;
    }

    PRBool hasContinue = PR_FALSE;
   strpos = strbuf;
    *strpos = '\0';
    
    // hasContinue == whether we have a child with a mCon
    t = aNode->mChild;
    while (t && !hasContinue) {
      hasContinue = t->mCon;
      t = t->mSibling;
    }

#ifdef DEBUG_XF_ANALYZER
    for (PRUint32 j = 0; j < aIndent; ++j) {
      strpos += sprintf(strpos, "  ");
    }
    
    strpos += sprintf(strpos, "<%s> ", hasContinue ? "C" : " ");
    
    if (aNode->mPredicate) {
      if (!(aNode->mChild)) {
        strpos += sprintf(strpos, "[PredicateVal], ");
      } else {
        strpos += sprintf(strpos, "[Predicate], ");
      }
    } else {
      if (!(aNode->mChild)) {
        strpos += sprintf(strpos, "[AxisStepsVal], ");
      } else {
        if (!hasContinue) {
          strpos += printf(strpos, "[AxisSteps], ");
        }
      }
    }
#endif

    if (aNode->mCon) {
      // Remove the leading /
      xp = Substring(*mCurExprString, aNode->mStartIndex + 1, aNode->mEndIndex - aNode->mStartIndex + 1);
    } else {
      xp = Substring(*mCurExprString, aNode->mStartIndex, aNode->mEndIndex - aNode->mStartIndex);
    }

    rv = mEvaluator->Evaluate(xp, aContextNode, mResolver,
                              nsIDOMXPathResult::ANY_TYPE,
                              nsnull, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 type;
    rv = result->GetResultType(&type);
    NS_ENSURE_SUCCESS(rv, rv);

    // We are only interested in nodes
    if (   type != nsIDOMXPathResult::UNORDERED_NODE_ITERATOR_TYPE
        && type != nsIDOMXPathResult::ORDERED_NODE_ITERATOR_TYPE) {
      continue;
    }

    while (NS_SUCCEEDED(result->IterateNext(getter_AddRefs(node))) && node) {
      NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_XF_ANALYZER
      printf(strbuf);
#endif
      if (aNode->mChild || (!aNode->mChild && hasContinue)) {
#ifdef DEBUG_XF_ANALYZER
        printf("iterating '%s'\n", NS_ConvertUCS2toUTF8(xp).get());
#endif
      } else {
#ifdef DEBUG_XF_ANALYZER
        printf("collecting '%s'\n", NS_ConvertUCS2toUTF8(xp).get());
#endif
        mCurSet->AddNode(node);
      }
      rv = AnalyzeRecursively(node, aNode->mChild, aIndent + 1);
    }

    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}
