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
#include "nsXFormsUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIXPathEvaluatorInternal.h"

#ifdef DEBUG
//#define DEBUG_XF_ANALYZER
#endif

nsXFormsXPathAnalyzer::nsXFormsXPathAnalyzer(nsIXPathEvaluatorInternal *aEvaluator,
                                             nsIDOMXPathNSResolver     *aResolver,
                                             nsIXFormsXPathState       *aState)
  : mEvaluator(aEvaluator),
    mResolver(aResolver),
    mState(aState)
{
  MOZ_COUNT_CTOR(nsXFormsXPathAnalyzer);
}

nsXFormsXPathAnalyzer::~nsXFormsXPathAnalyzer()
{
  MOZ_COUNT_DTOR(nsXFormsXPathAnalyzer);
}

nsresult
nsXFormsXPathAnalyzer::Analyze(nsIDOMNode                *aContextNode,
                               const nsXFormsXPathNode   *aNode,
                               nsIDOMNSXPathExpression   *aExpression,
                               const nsAString           *aExprString,
                               nsCOMArray<nsIDOMNode>    *aSet,
                               PRUint32                   aPosition,
                               PRUint32                   aSize,
                               PRBool                     aIncludeRoot)
{
  NS_ENSURE_ARG(aContextNode);
  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG(aExpression);
  NS_ENSURE_ARG(aExprString);
  NS_ENSURE_ARG(aSet);

  mCurExpression = aExpression;
  mCurExprString = aExprString;
  mCurSet = aSet;
  mCurSize = aSize;
  mCurPosition = aPosition;

#ifdef DEBUG_XF_ANALYZER
  printf("=====================================\n");
  printf("Analyzing: %s\n", NS_ConvertUTF16toUTF8(*mCurExprString).get());
  printf("=====================================\n");
#endif

  nsresult rv = AnalyzeRecursively(aContextNode, aNode->mChild, 0,
                                   aIncludeRoot);
#ifdef DEBUG_XF_ANALYZER
  printf("-------------------------------------\n");
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  // Besides making the set a set, it also makes it sorted.
  nsXFormsUtils::MakeUniqueAndSort(aSet);

  return NS_OK;
}


nsresult
nsXFormsXPathAnalyzer::AnalyzeRecursively(nsIDOMNode              *aContextNode,
                                          const nsXFormsXPathNode *aNode,
                                          PRUint32                aIndent,
                                          PRBool                  aCollect)
{
  nsXFormsXPathNode* t;
  nsAutoString xp;
  nsresult rv;
  nsCOMPtr<nsIDOMXPathResult> result;
  nsCOMPtr<nsIDOMNode> node;
#ifdef DEBUG_XF_ANALYZER
  char strbuf[100];
  char* strpos;
#endif

#ifdef DEBUG_beaufour_xxx
  printf("nsXFormsXPathParser::AnalyzeRecursively(%p)\n", (void*) aNode);
#endif
  
  for (; aNode; aNode = aNode->mSibling) {
#ifdef DEBUG_beaufour_xxx
    printf("\tChild: %p, Sibling: %p\n", (void*) aNode->mChild, (void*) aNode->mSibling);
    printf("\tIndex: %d - %d\n", aNode->mStartIndex, aNode->mEndIndex);
    printf("\tCon: %d, Predicate: %d, Literal: %d\n", aNode->mCon, aNode->mPredicate, aNode->mLiteral);
    printf("\tIsIndex: %d\n", aNode->mIsIndex);
#endif

    if (aNode->mEndIndex < 0 || aNode->mStartIndex >= aNode->mEndIndex) {
      continue;
    }

    PRBool hasContinue = PR_FALSE;
    
    // hasContinue == whether we have a child with a mCon
    t = aNode->mChild;
    while (t && !hasContinue) {
      hasContinue = t->mCon;
      t = t->mSibling;
    }

#ifdef DEBUG_XF_ANALYZER
    strpos = strbuf;
    *strpos = '\0';
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
      xp = Substring(*mCurExprString, aNode->mStartIndex + 1,
                     aNode->mEndIndex - aNode->mStartIndex - 1);
    } else {
      xp = Substring(*mCurExprString, aNode->mStartIndex,
                     aNode->mEndIndex - aNode->mStartIndex);
    }

    // It is an error to use an evaluator from a different document than the
    // context node.  The context node can change as we recurse and be
    // from an different instance document than the context node we used to
    // kick off the evaluation.  More efficient to just get the evaluator every
    // time we recurse rather than caching it and testing.

    nsCOMPtr<nsIDOMDocument> doc;
    aContextNode->GetOwnerDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMXPathEvaluator> eval = do_QueryInterface(doc);
    nsCOMPtr<nsIXPathEvaluatorInternal> evalInternal = do_QueryInterface(eval);
    NS_ENSURE_STATE(evalInternal);

    rv = nsXFormsUtils::EvaluateXPath(evalInternal, xp, aContextNode, mResolver,
                                      mState, nsIDOMXPathResult::ANY_TYPE,
                                      mCurPosition, mCurSize, nsnull,
                                      getter_AddRefs(result));
    if (NS_FAILED(rv)) {
      const PRUnichar *strings[] = { xp.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("exprEvaluateError"),
                                 strings, 1, nsnull, nsnull);
      return rv;
    }

    PRUint16 type;
    rv = result->GetResultType(&type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aNode->mIsIndex) {
      // Extract index parameter, xp is "index(parameter)"
      const PRUint32 indexSize = sizeof("index(") - 1;
      nsDependentSubstring indexExpr = Substring(xp,
                                                 indexSize,
                                                 xp.Length() - indexSize - 1); // remove final ')' too

      nsCOMPtr<nsIDOMXPathResult> stringRes;
      rv = nsXFormsUtils::EvaluateXPath(evalInternal, indexExpr, aContextNode,
                                        mResolver, mState,
                                        nsIDOMXPathResult::STRING_TYPE,
                                        mCurPosition, mCurSize, nsnull,
                                        getter_AddRefs(stringRes));
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString indexId;
      rv = stringRes->GetStringValue(indexId);
      NS_ENSURE_SUCCESS(rv, rv);
      mIndexesUsed.AppendString(indexId);
    }

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
      if (!aCollect && (aNode->mChild || (!aNode->mChild && hasContinue))) {
#ifdef DEBUG_XF_ANALYZER
        printf("iterating '%s'\n", NS_ConvertUTF16toUTF8(xp).get());
#endif
      } else {
#ifdef DEBUG_XF_ANALYZER
        printf("collecting '%s'\n", NS_ConvertUTF16toUTF8(xp).get());
#endif
        mCurSet->AppendObject(node);
      }
      rv = AnalyzeRecursively(node, aNode->mChild, aIndent + 1);
    }

    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

const nsStringArray&
nsXFormsXPathAnalyzer::IndexesUsed() const
{
  return mIndexesUsed;
}
