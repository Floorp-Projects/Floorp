/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _IdlScanner_h__
#define _IdlScanner_h__

#include <string.h>

#ifdef XP_MAC
#include <fstream.h>			// required for namespace resolution
#else
class ifstream;
#endif

#define MAX_ID_LENGHT   256

enum EIDLTokenType {
  EOF_TOKEN = -1,
  ERROR_TOKEN = 0,
  COMMENT_TOKEN = 1,
  INTERFACE_TOKEN,
  TYPEDEF_TOKEN,
  STRUCT_TOKEN,
  ENUM_TOKEN,
  UNION_TOKEN,
  CONST_TOKEN,
  EXCEPTION_TOKEN,
  READONLY_TOKEN,
  OPTIONAL_TOKEN,
  XPIDL_TOKEN,
  FUNC_TOKEN,
  ELLIPSIS_TOKEN,
  IID_TOKEN,
  ATTRIBUTE_TOKEN,
  IDENTIFIER_TOKEN,
  BOOLEAN_TOKEN,
  FLOAT_TOKEN,
  DOUBLE_TOKEN,
  LONG_TOKEN,
  SHORT_TOKEN,
  ULONG_TOKEN,
  USHORT_TOKEN,
  CHAR_TOKEN,
  INT_TOKEN,
  UINT_TOKEN,
  STRING_TOKEN,
  INPUT_PARAM_TOKEN,
  OUTPUT_PARAM_TOKEN,
  INOUT_PARAM_TOKEN,
  RAISES_TOKEN,
  INHERITANCE_SPEC_TOKEN, // ':'
  SEPARATOR_TOKEN, // ','
  BEGIN_BLOCK_TOKEN, // '{'
  END_BLOCK_TOKEN, // '}'
  TERMINATOR_TOKEN, // ';'
  ASSIGNEMENT_TOKEN, // '='
  FUNC_PARAMS_SPEC_BEGIN_TOKEN, // '('
  FUNC_PARAMS_SPEC_END_TOKEN, // ')'
  VOID_TOKEN,
  // constant values
  INTEGER_CONSTANT = 1000,
  STRING_CONSTANT
};

struct Token {
  int id;
  char *stringID;
  union {
    // long is the only one used so far
    unsigned long vLong;
    // still unused...
    char vChar;
    char *vString;
    double vDouble;
  } value;

  Token() 
  {
    id = 0;
    stringID = (char*)0;
    memset(&value, 0, sizeof(value));
  }

  ~Token()
  {
    if (STRING_CONSTANT == id) {
      delete[] value.vString;
    }

    delete[] stringID;
  }

  void SetToken(int aID, char *aString = 0)
  {
    if (STRING_CONSTANT == id) {
      delete[] value.vString;
    }

    id = aID;

    if (stringID) {
      delete[] stringID;
    }

    if (aString) {
      stringID = new char[strlen(aString) + 1];
      memcpy(stringID, aString, strlen(aString) + 1);
    }
    else {
      stringID = (char*)0;
    }

    memset(&value, 0, sizeof(value));
  }

  void SetTokenValue(int aID, long aValue, char *aString = 0)
  {
    SetToken(aID, aString);
    value.vLong = (unsigned long)aValue;
  }

  void SetTokenValue(int aID, char aValue, char *aString = 0)
  {
    SetToken(aID, aString);
    value.vChar = aValue;
  }

  void SetTokenValue(int aID, char *aValue, char *aString = 0)
  {
    SetToken(aID, aString);
    value.vString = aValue;
  }

  void SetTokenValue(int aID, double aValue, char *aString = 0)
  {
    SetToken(aID, aString);
    value.vDouble = aValue;
  }

};

class IdlScanner {
private:
  ifstream *mInputFile;
  char *mFileName;
  Token *mToken1;
  Token *mToken2;
  Token *mCurrentToken;
  long mLineNumber;
  int mTokenPeeked;
  char mTokenName[MAX_ID_LENGHT];

public:
            IdlScanner();
            ~IdlScanner();

  char*     GetFileName();
  void      SetFileName(char *aFileName);
  long      GetLineNumber();

  int       Open(char *aFileName);
  int       CanReadMoreData();

  Token*    PeekToken();
  Token*    NextToken();

protected:
  void      SetCurrentToken();
  int       EatWhiteSpace();

  void      AKeywords(char *aCurrentPos, Token *aToken);
  void      BKeywords(char *aCurrentPos, Token *aToken);
  void      CKeywords(char *aCurrentPos, Token *aToken);
  void      DKeywords(char *aCurrentPos, Token *aToken);
  void      EKeywords(char *aCurrentPos, Token *aToken);
  void      FKeywords(char *aCurrentPos, Token *aToken);
  void      IKeywords(char *aCurrentPos, Token *aToken);
  void      LKeywords(char *aCurrentPos, Token *aToken);
  void      OKeywords(char *aCurrentPos, Token *aToken);
  void      RKeywords(char *aCurrentPos, Token *aToken);
  void      SKeywords(char *aCurrentPos, Token *aToken);
  void      TKeywords(char *aCurrentPos, Token *aToken);
  void      UKeywords(char *aCurrentPos, Token *aToken);
  void      VKeywords(char *aCurrentPos, Token *aToken);
  void      WKeywords(char *aCurrentPos, Token *aToken);
  void      XKeywords(char *aCurrentPos, Token *aToken);
  void      Identifier(char *aCurrentPos, Token *aToken);
  void      Number(int aStartChar, Token *aToken);
  void      String(int aStartChar, Token *aToken);
  void      Char(int aStartChar, Token *aToken);
  void      Comment(char *aCurrentPos, Token *aToken);
  void      KeywordMismatch(int aChar, char *aCurrentPos, Token *aToken);
};


#endif // _IdlScanner_h__

