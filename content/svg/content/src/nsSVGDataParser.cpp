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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /**
  * NOTE:
  *
  *   How should subclasses use this class?
  *   This class was separated from the nsSVGPathDataParser class to share
  *   functionality found to be common in other descent parsers in this component.
  *
  *   A subclass should implement a Match method which gets invoked from the
  *   Parse method.  The Parse method can be overridden, as required.
  *
  */


#include "nsSVGDataParser.h"
#include "prdtoa.h"
#include "nsSVGUtils.h"
#include "nsMathUtils.h"
#include <stdlib.h>
#include <math.h>

//----------------------------------------------------------------------
// public interface

nsresult
nsSVGDataParser::Parse(const nsAString &aValue)
{
  char *str = ToNewUTF8String(aValue);
  if (!str)
    return NS_ERROR_OUT_OF_MEMORY;

  mInputPos = str;

  GetNextToken();
  nsresult rv = Match();
  if (mTokenType != END)
    rv = NS_ERROR_FAILURE; // not all tokens were consumed

  mInputPos = nsnull;
  nsMemory::Free(str);

  return rv;
}

//----------------------------------------------------------------------
// helpers

void nsSVGDataParser::GetNextToken()
{
  mTokenPos = mInputPos;
  mTokenVal = *mInputPos;
  
  switch (mTokenVal) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      mTokenType = DIGIT;
      break;
    case '\x20': case '\x9': case '\xd': case '\xa':
      mTokenType = WSP;
      break;
    case ',':
      mTokenType = COMMA;
      break;
    case '+': case '-':
      mTokenType = SIGN;
      break;
    case '.':
      mTokenType = POINT;
      break;
    case '(':
      mTokenType = LEFT_PAREN;
      break;
    case ')':
      mTokenType = RIGHT_PAREN;
      break;
    case '\0':
      mTokenType = END;
      break;
    default:
      mTokenType = OTHER;
  }
  
  if (*mInputPos != '\0') {
    ++mInputPos;
  }
}

void nsSVGDataParser::RewindTo(const char* aPos)
{
  mInputPos = aPos;
  GetNextToken();
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchNonNegativeNumber(float* aX)
{
  // XXX inefficient implementation. We probably hit the RewindTo case
  // often.
  
  const char* pos = mTokenPos;

  nsresult rv = MatchFloatingPointConst();

  if (NS_FAILED(rv)) {
    RewindTo(pos);
    ENSURE_MATCHED(MatchIntegerConst());
  }

  char* end;
  *aX = float(PR_strtod(pos, &end));
  if (pos != end && NS_finite(*aX)) {
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

PRBool nsSVGDataParser::IsTokenNonNegativeNumberStarter()
{
  return (mTokenType == DIGIT || mTokenType == POINT);
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchNumber(float* aX)
{
  const char* pos = mTokenPos;
  
  if (mTokenType == SIGN)
    GetNextToken();

  const char* pos2 = mTokenPos;

  nsresult rv = MatchFloatingPointConst();

  if (NS_FAILED(rv)) {
    RewindTo(pos2);
    ENSURE_MATCHED(MatchIntegerConst());
  }

  char* end;
  /* PR_strtod is not particularily fast. We only need a float and not a double so
   * we could probably use something faster here if needed. The CSS parser uses
   * nsCSSScanner::ParseNumber() instead of PR_strtod. See bug 516396 for some
   * additional info. */
  *aX = float(PR_strtod(pos, &end));
  if (pos != end && NS_finite(*aX)) {
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

PRBool nsSVGDataParser::IsTokenNumberStarter()
{
  return (mTokenType == DIGIT || mTokenType == POINT || mTokenType == SIGN);
}


//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchCommaWsp()
{
  switch (mTokenType) {
    case WSP:
      ENSURE_MATCHED(MatchWsp());
      if (mTokenType == COMMA)
        GetNextToken();
      break;
    case COMMA:
      GetNextToken();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  while (IsTokenWspStarter()) {
    ENSURE_MATCHED(MatchWsp());
  }
  return NS_OK;
}
  
PRBool nsSVGDataParser::IsTokenCommaWspStarter()
{
  return (IsTokenWspStarter() || mTokenType == COMMA);
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchIntegerConst()
{
  ENSURE_MATCHED(MatchDigitSeq());
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchFloatingPointConst()
{
  // XXX inefficient implementation. It would be nice if we could make
  // this predictive and wouldn't have to backtrack...
  
  const char* pos = mTokenPos;

  nsresult rv = MatchFractConst();
  if (NS_SUCCEEDED(rv)) {
    if (IsTokenExponentStarter())
      ENSURE_MATCHED(MatchExponent());
  }
  else {
    RewindTo(pos);
    ENSURE_MATCHED(MatchDigitSeq());
    ENSURE_MATCHED(MatchExponent());    
  }

  return NS_OK;  
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchFractConst()
{
  if (mTokenType == POINT) {
    GetNextToken();
    ENSURE_MATCHED(MatchDigitSeq());
  }
  else {
    ENSURE_MATCHED(MatchDigitSeq());
    if (mTokenType == POINT) {
      GetNextToken();
      if (IsTokenDigitSeqStarter()) {
        ENSURE_MATCHED(MatchDigitSeq());
      }
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchExponent()
{
  if (!(tolower(mTokenVal) == 'e')) return NS_ERROR_FAILURE;

  GetNextToken();

  if (mTokenType == SIGN)
    GetNextToken();

  ENSURE_MATCHED(MatchDigitSeq());

  return NS_OK;  
}

PRBool nsSVGDataParser::IsTokenExponentStarter()
{
  return (tolower(mTokenVal) == 'e');
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchDigitSeq()
{
  if (!(mTokenType == DIGIT)) return NS_ERROR_FAILURE;

  do {
    GetNextToken();
  } while (mTokenType == DIGIT);

  return NS_OK;
}

PRBool nsSVGDataParser::IsTokenDigitSeqStarter()
{
  return (mTokenType == DIGIT);
}

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchWsp()
{
  if (!(mTokenType == WSP)) return NS_ERROR_FAILURE;

  do {
    GetNextToken();
  } while (mTokenType == WSP);

  return NS_OK;  
}

PRBool nsSVGDataParser::IsTokenWspStarter()
{
  return (mTokenType == WSP);
}  

//----------------------------------------------------------------------

nsresult nsSVGDataParser::MatchLeftParen()
{
  switch (mTokenType) {
    case LEFT_PAREN:
      GetNextToken();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

 
  return NS_OK;
}

nsresult nsSVGDataParser::MatchRightParen()
{
  switch (mTokenType) {
    case RIGHT_PAREN:
       GetNextToken();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

