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

#ifndef __NSXFORMSXPATHSCANNER_H__
#define __NSXFORMSXPATHSCANNER_H__

#include "nsString.h"

/**
 * The XPath Expression scanner, used by nsXFormsXPathParser.
 * 
 * @note Should be reimplemented and moved to Transformiix
 * 
 */
class nsXFormsXPathScanner
{
public:
  enum XPATHTOKEN {
    LPARAN, RPARAN, LBRACK, RBRACK, AT, COMMA,
    COLONCOLON, DOT, DOTDOT, SLASH, SLASHSLASH, UNION,
    PLUS, MINUS, EQUAL, NOTEQUAL, LEQUAL, LESS, GEQUAL,
    GREATER, MULTIPLYOPERATOR, WHITESPACE, NUMBER,
    LITERAL, AND, OR, MOD, DIV, NODE, PI, TEXT, COMMENT,
    FUNCTIONNAME, NCNAME, QNAME, VARIABLE, STAR, ANCESTOR,
    ANCESTOR_OR_SELF, ATTRIBUTE, CHILD, DESCENDANT,
    DESCENDANT_OR_SELF, FOLLOWING, FOLLOWING_SIBLING, NAMESPACE,
    PARENT, PRECEDING, PRECEDING_SIBLING, SELF, XPATHEOF, ERRORXPATHTOKEN, NONE
  };

private:
  XPATHTOKEN mState, mLast;
  nsAutoString mExpression;
  PRInt32 mOffset, mLength, mSize;

  PRUnichar PopChar();
  PRUnichar PeekChar();
  PRUnichar PeekChar(PRInt32 offset);
  PRUnichar NextNonWhite();
  PRInt32 GetOffsetForNonWhite();

  PRBool SolveDiambiguate();
  XPATHTOKEN SolveStar();
  XPATHTOKEN ScanNumber();
  XPATHTOKEN ScanVariable();
  XPATHTOKEN ScanWhitespace();
  XPATHTOKEN ScanLiteral();
  XPATHTOKEN ScanQName();
  XPATHTOKEN ScanNCName();


public:
  nsXFormsXPathScanner();
  nsXFormsXPathScanner(const nsAString& aExpression);
  ~nsXFormsXPathScanner();

  void Init(const nsAString& aExpression);
  XPATHTOKEN NextToken();
  PRInt32 Offset();
  PRInt32 Length();
  void Image(PRInt32& aOffset, PRInt32& aLength);
  PRBool HasMoreTokens();
  nsAString& Expression();
};

#endif
