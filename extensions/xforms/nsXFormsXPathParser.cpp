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

#include "nsXFormsXPathParser.h"
#include "nscore.h"

/*
 * (TODO) GIANT hack. Without exceptions everything had to be rewritten, and
 * that was to overdo it a bit, when the code probably does not survive...
 */
void
XPathCompilerException(const char* aMsg, nsAString& aExpression, PRInt32 aOffset = -1, PRInt32 aLength = -1) {
  printf("XPathCompilerException: %s, %s [o: %d, l: %d]\n",
         aMsg,
         NS_ConvertUCS2toUTF8(aExpression).get(), aOffset, aLength);
         
  printf("WARNING: Houston we have a problem, and unlike Apollo 13, we're not going to make it!\n");
  NS_ABORT();
}


MOZ_DECL_CTOR_COUNTER(nsXFormsXPathParser)

nsXFormsXPathParser::nsXFormsXPathParser()
    : mUsesDynamicFunc(PR_FALSE), mHead(nsnull), mAnalyzeStackPointer(nsnull)
{
  MOZ_COUNT_CTOR(nsXFormsXPathParser);
}

nsXFormsXPathParser::~nsXFormsXPathParser()
{
  MOZ_COUNT_DTOR(nsXFormsXPathParser);
}

void
nsXFormsXPathParser::PushContext(PRInt32 aStartIndex)
{
  mHead = new nsXFormsXPathNode(mHead);
  if (aStartIndex == -100) {
    mHead->mStartIndex = mScanner.Offset() + 1;
  } else {
    mHead->mStartIndex = aStartIndex;
  }
  mStack[++mAnalyzeStackPointer] = mHead;
  mHead->mPredicate = mPredicateLevel != 0;
  mHead->mLiteral = PR_FALSE;
}

void
nsXFormsXPathParser::PushContext(nsXFormsXPathNode* aNode)
{
  mStack[++mAnalyzeStackPointer] = aNode;
  mHead = aNode;
}

void
nsXFormsXPathParser::EndContext()
{
  if (mHead) {
    mHead->mEndIndex = mScanner.Offset() + 1;
  }
}

void
nsXFormsXPathParser::RestartContext()
{
  nsXFormsXPathNode* temp = new nsXFormsXPathNode(nsnull, PR_TRUE);
  temp->mStartIndex = mScanner.Offset() + 1;
  temp->mSibling = mHead->mChild;
  temp->mPredicate = mPredicateLevel != 0;
  mHead->mChild = temp;
  mHead = temp;
  mStack[mAnalyzeStackPointer] = mHead;
}

nsXFormsXPathNode*
nsXFormsXPathParser::JustContext()
{
  nsXFormsXPathNode* t = mStack[mAnalyzeStackPointer];
  mHead = mStack[--mAnalyzeStackPointer];
  return t;
}


nsXFormsXPathNode*
nsXFormsXPathParser::PopContext()
{
  if (mHead->mEndIndex == -100) {
    mHead->mEndIndex = mScanner.Offset() + 1;
  }

  NS_ASSERTION(mHead->mStartIndex <= mHead->mEndIndex, "");
  NS_ASSERTION(mAnalyzeStackPointer - 1 >= 0, "");
  mHead = mStack[--mAnalyzeStackPointer];

  return mHead;
}

void
nsXFormsXPathParser::AbbreviatedStep()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::DOT:
    PopToken();
    break;

  case nsXFormsXPathScanner::DOTDOT:
    PopToken();
    break;

  default:
    XPathCompilerException("Expected . or ..", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
    break;
  }
}

void
nsXFormsXPathParser::AbsoluteLocationPath()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  nsXFormsXPathScanner::XPATHTOKEN r;
  switch (t) {
  case nsXFormsXPathScanner::SLASH:
    PopToken();
    r = PeekToken();
    switch (r) {
    case nsXFormsXPathScanner::ANCESTOR:
    case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
    case nsXFormsXPathScanner::ATTRIBUTE:
    case nsXFormsXPathScanner::CHILD:
    case nsXFormsXPathScanner::DESCENDANT:
    case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
    case nsXFormsXPathScanner::FOLLOWING:
    case nsXFormsXPathScanner::FOLLOWING_SIBLING:
    case nsXFormsXPathScanner::NAMESPACE:
    case nsXFormsXPathScanner::PARENT:
    case nsXFormsXPathScanner::PRECEDING:
    case nsXFormsXPathScanner::PRECEDING_SIBLING:
    case nsXFormsXPathScanner::SELF:
    case nsXFormsXPathScanner::AT:
    case nsXFormsXPathScanner::STAR:
    case nsXFormsXPathScanner::NCNAME:
    case nsXFormsXPathScanner::QNAME:
    case nsXFormsXPathScanner::COMMENT:
    case nsXFormsXPathScanner::TEXT:
    case nsXFormsXPathScanner::PI:
    case nsXFormsXPathScanner::NODE:
    case nsXFormsXPathScanner::DOT:
    case nsXFormsXPathScanner::DOTDOT:
      RelativeLocationPath();
      break;
      
    default:
      break;
    }
    break;

  case nsXFormsXPathScanner::SLASHSLASH:
    PopToken();
    if (DoRelative()) {
      RelativeLocationPath();
    }
    break;

  default:
    XPathCompilerException("Not an absolute location path", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
}

void
nsXFormsXPathParser::AdditiveExpression()
{
  PRBool con = PR_TRUE;

  MultiplicationExpr();

  while (con) {
    nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
    switch (t) {
    case nsXFormsXPathScanner::PLUS:
      PopToken();
      MultiplicationExpr();
      break;

    case nsXFormsXPathScanner::MINUS:
      PopToken();
      MultiplicationExpr();
      break;

    default:
      con = PR_FALSE;
    }
  }
}

void
nsXFormsXPathParser::AndExpr()
{
  EqualityExpr();
  if (PeekToken() == nsXFormsXPathScanner::AND) {
    PopToken();
    AndExpr();
  }
}

void
nsXFormsXPathParser::AxisSpecifier()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::ANCESTOR:
  case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
  case nsXFormsXPathScanner::ATTRIBUTE:
  case nsXFormsXPathScanner::CHILD:
  case nsXFormsXPathScanner::DESCENDANT:
  case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
  case nsXFormsXPathScanner::FOLLOWING:
  case nsXFormsXPathScanner::FOLLOWING_SIBLING:
  case nsXFormsXPathScanner::NAMESPACE:
  case nsXFormsXPathScanner::PARENT:
  case nsXFormsXPathScanner::PRECEDING:
  case nsXFormsXPathScanner::PRECEDING_SIBLING:
  case nsXFormsXPathScanner::SELF:
    PopToken();
  case nsXFormsXPathScanner::AT:
    PopToken();
    break;
    
  default:
    XPathCompilerException("Not a axis specifier", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
    break;
  }
}

PRBool
nsXFormsXPathParser::DoRelative()
{
  PRBool ret = PR_FALSE;

  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::ANCESTOR:
  case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
  case nsXFormsXPathScanner::ATTRIBUTE:
  case nsXFormsXPathScanner::CHILD:
  case nsXFormsXPathScanner::DESCENDANT:
  case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
  case nsXFormsXPathScanner::FOLLOWING:
  case nsXFormsXPathScanner::FOLLOWING_SIBLING:
  case nsXFormsXPathScanner::NAMESPACE:
  case nsXFormsXPathScanner::PARENT:
  case nsXFormsXPathScanner::PRECEDING:
  case nsXFormsXPathScanner::PRECEDING_SIBLING:
  case nsXFormsXPathScanner::SELF:
  case nsXFormsXPathScanner::AT:
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
  case nsXFormsXPathScanner::COMMENT:
  case nsXFormsXPathScanner::TEXT:
  case nsXFormsXPathScanner::PI:
  case nsXFormsXPathScanner::NODE:
  case nsXFormsXPathScanner::DOT:
  case nsXFormsXPathScanner::DOTDOT:
    ret = PR_TRUE;
    break;
    
  default:
    break;
  }
  return ret;
}

void
nsXFormsXPathParser::EqualityExpr()
{
  RelationalExpression();

  PRBool con = PR_TRUE;
  while (con) {
    nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
    switch (t) {
    case nsXFormsXPathScanner::EQUAL:
      PopToken();
      RelationalExpression();
      break;

    case nsXFormsXPathScanner::NOTEQUAL:
      PopToken();
      RelationalExpression();
      break;

    default:
      con = PR_FALSE;
    }
  }
}

void
nsXFormsXPathParser::Expr()
{
  OrExpr();
}

void
nsXFormsXPathParser::FilterExpr()
{
  if (PrimaryExpr() && PeekToken() == nsXFormsXPathScanner::LBRACK) {
      Predicate();
  }
}

void
nsXFormsXPathParser::FunctionCall()
{
  if (!mUsesDynamicFunc) {
    nsDependentSubstring fname = Substring(mScanner.Expression(), mScanner.Offset() + 1, mScanner.Offset() + mScanner.Length() + 1);
    if (fname.Equals(NS_LITERAL_STRING("now"))) {
      mUsesDynamicFunc = PR_TRUE;
    }
  }
  PopToken();
  PopToken();
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();

  PRBool con = t == nsXFormsXPathScanner::RPARAN ? PR_FALSE : PR_TRUE;
  while (con) {
    if (t == nsXFormsXPathScanner::XPATHEOF) {
      XPathCompilerException("Expected ) got EOF", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
    }
    nsXFormsXPathNode* c = JustContext();
    Expr();
    PushContext(c);

    t = PeekToken();
    if (t == nsXFormsXPathScanner::COMMA) {
      PopToken(); // Another Argument
      con = PR_TRUE;
    } else {
      con = PR_FALSE;
    }
  }

  if (t != nsXFormsXPathScanner::RPARAN) {
    XPathCompilerException("Expected )", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }

  PopToken();
}

void
nsXFormsXPathParser::LocationPath()
{
  if (DoRelative()) {
    PushContext();
    RelativeLocationPath();
    PopContext();
  } else {
    nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
    switch (t) {
    case nsXFormsXPathScanner::SLASH:
    case nsXFormsXPathScanner::SLASHSLASH:
      PushContext();
      AbsoluteLocationPath();
      PopContext();
      break;

    default:
      XPathCompilerException("Not a location path", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
    }
  }
}

void
nsXFormsXPathParser::MultiplicationExpr()
{
  UnaryExpr();
  PRBool con = PR_TRUE;
  while (con) {
    nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
    switch (t) {
    case nsXFormsXPathScanner::MULTIPLYOPERATOR:
    case nsXFormsXPathScanner::DIV:
    case nsXFormsXPathScanner::MOD:
      PopToken();
      UnaryExpr();
      break;
    default:
      con = PR_FALSE;
    }
  }
}

void
nsXFormsXPathParser::NameTest()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
    PopToken();
    break;
  default:
    XPathCompilerException("NodeTest error", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
}

void
nsXFormsXPathParser::NodeTest()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
    NameTest();
    break;
  case nsXFormsXPathScanner::COMMENT:
  case nsXFormsXPathScanner::TEXT:
  case nsXFormsXPathScanner::PI:
  case nsXFormsXPathScanner::NODE:
    NodeType();
    break;
  default:
    XPathCompilerException("Not a node test", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
}

void
nsXFormsXPathParser::NodeType()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::COMMENT:
    PopToken();
    PopToken();
    PopToken();
    break;
  case nsXFormsXPathScanner::TEXT:
    PopToken();
    PopToken();
    PopToken();
    break;
  case nsXFormsXPathScanner::PI:
    PopToken();
    if ((t = PeekToken()) == nsXFormsXPathScanner::LPARAN) {
      PopToken();
      PopToken();
      PopToken();
    }
    break;

  case nsXFormsXPathScanner::NODE:
    PopToken();
    PopToken();
    PopToken();
    break;

  default:
    XPathCompilerException("Not a valid NodeType", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
}

void
nsXFormsXPathParser::OrExpr()
{
  AndExpr();
  if (PeekToken() == nsXFormsXPathScanner::OR) {
    PopToken();
    OrExpr();
  }
}

void
nsXFormsXPathParser::PathExpr()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::ANCESTOR:
  case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
  case nsXFormsXPathScanner::ATTRIBUTE:
  case nsXFormsXPathScanner::CHILD:
  case nsXFormsXPathScanner::DESCENDANT:
  case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
  case nsXFormsXPathScanner::FOLLOWING:
  case nsXFormsXPathScanner::FOLLOWING_SIBLING:
  case nsXFormsXPathScanner::NAMESPACE:
  case nsXFormsXPathScanner::PARENT:
  case nsXFormsXPathScanner::PRECEDING:
  case nsXFormsXPathScanner::PRECEDING_SIBLING:
  case nsXFormsXPathScanner::SELF:
  case nsXFormsXPathScanner::AT:
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
  case nsXFormsXPathScanner::COMMENT:
  case nsXFormsXPathScanner::TEXT:
  case nsXFormsXPathScanner::PI:
  case nsXFormsXPathScanner::NODE:
  case nsXFormsXPathScanner::DOT:
  case nsXFormsXPathScanner::DOTDOT:
  case nsXFormsXPathScanner::SLASH:
  case nsXFormsXPathScanner::SLASHSLASH:
    LocationPath();
    break;
    
  default:
    PushContext();
    FilterExpr();
    t = PeekToken();
    if (t == nsXFormsXPathScanner::SLASH || t == nsXFormsXPathScanner::SLASHSLASH) {
      PopToken();

      if (DoRelative()) {
        RelativeLocationPath();
      } else {
        nsAutoString nullstr;
        XPathCompilerException("After / in a filter expression it is required to have a reletive path expression", nullstr);
      }

    }
    PopContext();
  }
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathParser::PeekToken()
{
  return mPeek;
}

nsXFormsXPathScanner::XPATHTOKEN
nsXFormsXPathParser::PopToken()
{
  nsXFormsXPathScanner::XPATHTOKEN temp = mPeek;
  mPeek = mScanner.NextToken();
  if (mPeek == nsXFormsXPathScanner::WHITESPACE) { // Skip whitespaces
    mPeek = mScanner.NextToken();
  }
  return temp;
}


void
nsXFormsXPathParser::Predicate()
{
  EndContext();
  mPredicateLevel++;
  while (PeekToken() == nsXFormsXPathScanner::LBRACK) {
    PopToken();
    Expr();

    nsXFormsXPathScanner::XPATHTOKEN t = PopToken();
    if (t != nsXFormsXPathScanner::RBRACK) {
      XPathCompilerException("Expected ]", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
    }
  }
  mPredicateLevel--;
  RestartContext();
}

PRBool
nsXFormsXPathParser::PrimaryExpr()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::VARIABLE:
    PopToken();
    break;
    
  case nsXFormsXPathScanner::LPARAN: {
      PopToken();
      nsXFormsXPathNode* c = JustContext();
      Expr();
      PushContext(c);
      if (PeekToken() == nsXFormsXPathScanner::RPARAN) {
        PopToken(); 
      } else {
        XPathCompilerException("Expected )", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
      }
    }
    break;
    
  case nsXFormsXPathScanner::LITERAL:
    mHead->mLiteral = PR_TRUE;
    PopToken();
    break;
    
  case nsXFormsXPathScanner::NUMBER:
    mHead->mLiteral = PR_TRUE;
    PopToken();
    break;
    
  case nsXFormsXPathScanner::FUNCTIONNAME:
    FunctionCall();
    return PeekToken() == nsXFormsXPathScanner::SLASH || PeekToken() == nsXFormsXPathScanner::SLASHSLASH || PeekToken() == nsXFormsXPathScanner::LBRACK;
    break;
    
  default:
    XPathCompilerException("Not a primary expression", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
  
  return PR_FALSE;
}

void
nsXFormsXPathParser::RelationalExpression()
{
  AdditiveExpression();
  PRBool con = PR_TRUE;
  while (con) {
    nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
    switch (t) {
    case nsXFormsXPathScanner::LESS:
      PopToken();
      AdditiveExpression();
      break;

    case nsXFormsXPathScanner::LEQUAL:
      PopToken();
      AdditiveExpression();
      break;

    case nsXFormsXPathScanner::GREATER:
      PopToken();
      AdditiveExpression();
      break;

    case nsXFormsXPathScanner::GEQUAL:
      PopToken();
      AdditiveExpression();
      break;
      
    default:
      con = PR_FALSE;
    }
  }
}

void
nsXFormsXPathParser::RelativeLocationPath()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::ANCESTOR:
  case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
  case nsXFormsXPathScanner::ATTRIBUTE:
  case nsXFormsXPathScanner::CHILD:
  case nsXFormsXPathScanner::DESCENDANT:
  case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
  case nsXFormsXPathScanner::FOLLOWING:
  case nsXFormsXPathScanner::FOLLOWING_SIBLING:
  case nsXFormsXPathScanner::NAMESPACE:
  case nsXFormsXPathScanner::PARENT:
  case nsXFormsXPathScanner::PRECEDING:
  case nsXFormsXPathScanner::PRECEDING_SIBLING:
  case nsXFormsXPathScanner::SELF:
  case nsXFormsXPathScanner::AT:
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
  case nsXFormsXPathScanner::COMMENT:
  case nsXFormsXPathScanner::TEXT:
  case nsXFormsXPathScanner::PI:
  case nsXFormsXPathScanner::NODE:
    Step();
    break;
    
  case nsXFormsXPathScanner::DOT:
  case nsXFormsXPathScanner::DOTDOT:
    AbbreviatedStep();
    break;

  default:
    XPathCompilerException("Not a relative location path ", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }

  t = PeekToken();
  
  if (t == nsXFormsXPathScanner::SLASH || t == nsXFormsXPathScanner::SLASHSLASH) {
    PopToken();
    if (DoRelative()) {
      RelativeLocationPath();
    }
  }
}

void
nsXFormsXPathParser::Step()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::ANCESTOR:
  case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
  case nsXFormsXPathScanner::ATTRIBUTE:
  case nsXFormsXPathScanner::CHILD:
  case nsXFormsXPathScanner::DESCENDANT:
  case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
  case nsXFormsXPathScanner::FOLLOWING:
  case nsXFormsXPathScanner::FOLLOWING_SIBLING:
  case nsXFormsXPathScanner::NAMESPACE:
  case nsXFormsXPathScanner::PARENT:
  case nsXFormsXPathScanner::PRECEDING:
  case nsXFormsXPathScanner::PRECEDING_SIBLING:
  case nsXFormsXPathScanner::SELF:
  case nsXFormsXPathScanner::AT:
    AxisSpecifier();
    break;

  default:
    break;
  }
  
  t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
  case nsXFormsXPathScanner::COMMENT:
  case nsXFormsXPathScanner::TEXT:
  case nsXFormsXPathScanner::PI:
  case nsXFormsXPathScanner::NODE:
    NodeTest();
    break;
    
  default:
    XPathCompilerException("Expected a NodeTest expression", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
  
  t = PeekToken();
  if (t == nsXFormsXPathScanner::LBRACK) {
    Predicate(); // set the predicates
  }
}

void
nsXFormsXPathParser::UnaryExpr()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::MINUS:
    PopToken();
    UnaryExpr();
    break;
    
  default:
    UnionExpr();
  }
}

void
nsXFormsXPathParser::UnionExpr()
{
  nsXFormsXPathScanner::XPATHTOKEN t = PeekToken();
  switch (t) {
  case nsXFormsXPathScanner::ANCESTOR:
  case nsXFormsXPathScanner::ANCESTOR_OR_SELF:
  case nsXFormsXPathScanner::ATTRIBUTE:
  case nsXFormsXPathScanner::CHILD:
  case nsXFormsXPathScanner::DESCENDANT:
  case nsXFormsXPathScanner::DESCENDANT_OR_SELF:
  case nsXFormsXPathScanner::FOLLOWING:
  case nsXFormsXPathScanner::FOLLOWING_SIBLING:
  case nsXFormsXPathScanner::NAMESPACE:
  case nsXFormsXPathScanner::PARENT:
  case nsXFormsXPathScanner::PRECEDING:
  case nsXFormsXPathScanner::PRECEDING_SIBLING:
  case nsXFormsXPathScanner::SELF:
  case nsXFormsXPathScanner::AT:
  case nsXFormsXPathScanner::STAR:
  case nsXFormsXPathScanner::NCNAME:
  case nsXFormsXPathScanner::QNAME:
  case nsXFormsXPathScanner::COMMENT:
  case nsXFormsXPathScanner::TEXT:
  case nsXFormsXPathScanner::PI:
  case nsXFormsXPathScanner::NODE:
  case nsXFormsXPathScanner::DOT:
  case nsXFormsXPathScanner::DOTDOT:
  case nsXFormsXPathScanner::SLASH:
  case nsXFormsXPathScanner::SLASHSLASH:
    PathExpr();
    t = PeekToken();
    if (t == nsXFormsXPathScanner::UNION) {
      PopToken();
      UnionExpr();
    }
    break;
    
  case nsXFormsXPathScanner::VARIABLE:
  case nsXFormsXPathScanner::LPARAN:
  case nsXFormsXPathScanner::LITERAL:
  case nsXFormsXPathScanner::NUMBER:
  case nsXFormsXPathScanner::FUNCTIONNAME:
    PathExpr();
    t = PeekToken();
    if (t == nsXFormsXPathScanner::UNION) {
      PopToken();
      UnionExpr();
    }
    break;
    
  default:
    XPathCompilerException("Unexpected union token", mScanner.Expression(), mScanner.Offset(), mScanner.Length());
  }
}

nsXFormsXPathNode*
nsXFormsXPathParser::Parse(const nsAString& aExpression)
{
#ifdef DEBUG_XF_PARSER
  printf("=====================================\n");
  printf("Parsing: %s\n", NS_ConvertUCS2toUTF8(aExpression).get());
  printf("=====================================\n");
#endif

  mScanner.Init(aExpression);
  mAnalyzeStackPointer = 0;
  mPredicateLevel = 0;
  mHead = nsnull;
  PopToken();
  PushContext();
  
  nsXFormsXPathNode* root = mHead;
  Expr();
  PopContext();

#ifdef DEBUG_XF_PARSER
  Dump(root);
  printf("-------------------------------------\n");
#endif

  return root;
}

PRBool
nsXFormsXPathParser::UsesDynamicFunc() const {
  return mUsesDynamicFunc;
}

#ifdef DEBUG_XF_PARSER
void
nsXFormsXPathParser::Dump(nsXFormsXPathNode* aNode)
{
  DumpWithLevel(aNode->mChild, 0);
}

void
nsXFormsXPathParser::DumpWithLevel(nsXFormsXPathNode* aNode, PRInt32 aLevel)
{
  while (aNode) {
    if (aNode->mStartIndex < aNode->mEndIndex) {
      for (int i = 0; i < aLevel; i++) {
        printf(" ");
      }
      if (aNode->mCon) {
        printf("(+)");
      } else {
        printf("(|)");
      }
      OutputSubExpression(aNode->mStartIndex, aNode->mEndIndex);
    }
    DumpWithLevel(aNode->mChild, aLevel + 2);
    aNode = aNode->mSibling;
  }
}

void
nsXFormsXPathParser::OutputSubExpression(PRInt32 aOffset, PRInt32 aEndOffset)
{
  const nsDependentSubstring expr = Substring(mScanner.Expression(), aOffset, aEndOffset - aOffset);
  printf("%s\n", NS_ConvertUCS2toUTF8(expr).get());
}
#endif
