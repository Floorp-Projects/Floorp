/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGDATAPARSER_H__
#define __NS_SVGDATAPARSER_H__

#include "nsError.h"
#include "nsStringGlue.h"

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
