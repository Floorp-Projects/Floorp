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

#include "nsXFormsXPathScanner.h"
#include "nsXFormsXPathNode.h"

#define ANALYZE_STACK_SIZE 120

/**
 * The XPath Expression parser. Uses nsXFormsXPathScanner to scan the expression
 * and builds a parse tree of nsXFormsXPathNode's.
 * 
 * @note Should be reimplemented and moved to Transformiix
 * 
 */
class nsXFormsXPathParser {
private:
  nsXFormsXPathScanner mScanner;
  nsXFormsXPathScanner::XPATHTOKEN mPeek;
  
  PRBool mUsesDynamicFunc;
  
  nsXFormsXPathNode* mHead;
  int mAnalyzeStackPointer;
  int mPredicateLevel;
  nsXFormsXPathNode* mStack[ANALYZE_STACK_SIZE];

  nsXFormsXPathScanner::XPATHTOKEN PeekToken();
  nsXFormsXPathScanner::XPATHTOKEN PopToken();

  void PushContext(PRInt32 aStartIndex = -100);
  void PushContext(nsXFormsXPathNode* mAnalyze);
  void EndContext();
  void RestartContext();
  nsXFormsXPathNode* JustContext();
  nsXFormsXPathNode* PopContext();
  
  PRBool DoRelative();

  void AbbreviatedStep();
  void AbsoluteLocationPath();
  void AdditiveExpression();
  void AndExpr();
  void AxisSpecifier();
  void EqualityExpr();
  void Expr();
  void FilterExpr();
  void FunctionCall();
  void LocationPath();
  void MultiplicationExpr();
  void NameTest();
  void NodeTest();
  void NodeType();
  void OrExpr();
  void PathExpr();
  void Predicate();
  PRBool PrimaryExpr();
  void RelationalExpression();
  void RelativeLocationPath();
  void Step();
  void UnaryExpr();
  void UnionExpr();

#ifdef DEBUG_XF_PARSER
  void Dump(nsXFormsXPathNode* aNode);
  void DumpWithLevel(nsXFormsXPathNode* aNode, PRInt32 aLevel);
  void OutputSubExpression(PRInt32 aOffset, PRInt32 aEndOffset);
#endif

public:
  nsXFormsXPathParser();
  ~nsXFormsXPathParser();
  
  nsXFormsXPathNode* Parse(const nsAString& aExpression);
  PRBool UsesDynamicFunc() const;
};
