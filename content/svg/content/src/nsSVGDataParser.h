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

#ifndef __NS_SVGDATAPARSER_H__
#define __NS_SVGDATAPARSER_H__

#include "nsCOMPtr.h"

//----------------------------------------------------------------------
// helper macros
#define ENSURE_MATCHED(exp) { nsresult rv = exp; if (NS_FAILED(rv)) return rv; }

////////////////////////////////////////////////////////////////////////
// nsSVGDataParser: a simple abstract class for parsing values
// for path and transform values.
// 
class nsSVGDataParser
{
public:
  nsresult Parse(const nsAString &aValue);

protected:
  const char* mInputPos;
  
  const char* mTokenPos;
  enum { DIGIT, WSP, COMMA, POINT, SIGN, LEFT_PAREN, RIGHT_PAREN, OTHER, END } mTokenType;
  char mTokenVal;

  // helpers
  void GetNextToken();
  void RewindTo(const char* aPos);
  virtual nsresult Match()=0;

  nsresult MatchNonNegativeNumber(float* aX);
  bool IsTokenNonNegativeNumberStarter();
  
  nsresult MatchNumber(float* x);
  bool IsTokenNumberStarter();
  
  nsresult MatchCommaWsp();
  bool IsTokenCommaWspStarter();
  
  nsresult MatchIntegerConst();
  
  nsresult MatchFloatingPointConst();
  
  nsresult MatchFractConst();
  
  nsresult MatchExponent();
  bool IsTokenExponentStarter();
  
  nsresult MatchDigitSeq();
  bool IsTokenDigitSeqStarter();
  
  nsresult MatchWsp();
  bool IsTokenWspStarter();

  nsresult MatchLeftParen();
  nsresult MatchRightParen();
};


#endif // __NS_SVGDATAPARSER_H__
