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

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsXFormsXPathNode.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIXPathEvaluatorInternal.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIXFormsXPathState.h"

/**
 * This class analyzes an XPath Expression parse tree (nsXFormsXPathNode), and
 * returns all the nodes that the expression depends on.
 * 
 * @note Should be reimplemented and moved to Transformiix
 * @note This class is not thread safe (mCur.*)
 */
class nsXFormsXPathAnalyzer {
private:
  nsCOMPtr<nsIXPathEvaluatorInternal> mEvaluator;
  nsCOMPtr<nsIDOMXPathNSResolver>     mResolver;
  nsCOMPtr<nsIXFormsXPathState>       mState;

  nsCOMArray<nsIDOMNode>              *mCurSet;
  nsCOMPtr<nsIDOMNSXPathExpression>    mCurExpression;
  const nsAString                     *mCurExprString;
  PRUint32                             mCurSize;
  PRUint32                             mCurPosition;
  nsStringArray                        mIndexesUsed;

  nsresult AnalyzeRecursively(nsIDOMNode              *aContextNode,
                              const nsXFormsXPathNode *aNode,
                              PRUint32                 aIndent,
                              PRBool                   aCollect = PR_FALSE);

public:
  nsXFormsXPathAnalyzer(nsIXPathEvaluatorInternal *aEvaluator,
                        nsIDOMXPathNSResolver     *aResolver,
                        nsIXFormsXPathState       *aState);
  ~nsXFormsXPathAnalyzer();
  
  nsresult Analyze(nsIDOMNode                *aContextNode,
                   const nsXFormsXPathNode   *aNode,
                   nsIDOMNSXPathExpression   *aExpression,
                   const nsAString           *aExprString,
                   nsCOMArray<nsIDOMNode>    *aSet,
                   PRUint32                   aSize,
                   PRUint32                   aPosition,
                   PRBool                     aIncludeRoot);

  const nsStringArray& IndexesUsed() const;
};
