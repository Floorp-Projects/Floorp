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

#include "IdlScanner.h"
#include <fstream.h>
#include <ctype.h>

#ifdef XP_MAC
#pragma warn_possunwant 		off
#endif

IdlScanner::IdlScanner()
{
  mFileName = (char*)0;
  mInputFile = new ifstream();
  mToken1 = new Token();
  mToken2 = new Token();
  mCurrentToken = (Token*)0;
  mTokenPeeked = 0;
  mLineNumber = 1;
  for (int i = 0; i < MAX_ID_LENGHT; i++) mTokenName[i] = 0;
}

IdlScanner::~IdlScanner()
{
  if (mFileName) {
    delete mFileName;
  }

  delete mInputFile;
  delete mToken1;
  delete mToken2;
}

char* IdlScanner::GetFileName()
{
  return mFileName;
}

void IdlScanner::SetFileName(char *aFileName)
{
  if (mFileName) {
    delete mFileName;
    mFileName = (char*)0;
  }

  if (aFileName) {
    size_t length = strlen(aFileName) + 1;
    mFileName = new char[length];
    strcpy(mFileName, aFileName);
  }
}

long IdlScanner::GetLineNumber()
{
  return mLineNumber;
}

int IdlScanner::Open(char *aFileName)
{
  mInputFile->close();
  mInputFile->clear();
#ifdef XP_MAC
  mInputFile->open(aFileName, ios::in);			// no 'nocreate' flag on Mac?
#else
  mInputFile->open(aFileName, ios::in | ios::nocreate);
#endif
  SetFileName(aFileName);
  return mInputFile->is_open();
}

int IdlScanner::CanReadMoreData()
{
#ifndef XP_MAC
  return !(mInputFile->eof());
#else
	return !(mInputFile->fail());			// not sure why EOF does not work
#endif
}

//
// Read the next token and return it.
// The difference with NextToken is that consecutive calls to PeekToken
// return the same token. 
// PeekToken is reset when NextToken is called.
// This way PeekToken acts like the single token look ahead needed by the parser
// in order to disambiguate the grammar in some situation.
// A sequence like - NextToken, PeekToken -
// returns two different tokens the parser can hold on to decide what to do.
//
Token* IdlScanner::PeekToken()
{
  if (mTokenPeeked == 0) {
    NextToken();
    mTokenPeeked = 1;
  }

  return mCurrentToken;
}

//
// Read the next token and return it.
//
Token* IdlScanner::NextToken()
{
  if (mTokenPeeked) {
    mTokenPeeked = 0;
  }
  else {
    SetCurrentToken();

    int c = EatWhiteSpace();
    if (EOF != c) {
      for (int i = 1; i < MAX_ID_LENGHT; i++) mTokenName[i] = 0;
      mTokenName[0] = c;

      mCurrentToken->SetToken(ERROR_TOKEN);

      switch(c) {
        case '(':
          mCurrentToken->SetToken(FUNC_PARAMS_SPEC_BEGIN_TOKEN);
          break;
        case ')':
          mCurrentToken->SetToken(FUNC_PARAMS_SPEC_END_TOKEN);
          break;
        case ',':
          mCurrentToken->SetToken(SEPARATOR_TOKEN);
          break;
        case '/':
          Comment(mTokenName + 1, mCurrentToken); // could be a comment
          break;
        case ':':
          mCurrentToken->SetToken(INHERITANCE_SPEC_TOKEN);
          break;
        case ';':
          mCurrentToken->SetToken(TERMINATOR_TOKEN);
          break;
        case '{':
          mCurrentToken->SetToken(BEGIN_BLOCK_TOKEN);
          break;
        case '}':
          mCurrentToken->SetToken(END_BLOCK_TOKEN);
          break;
        case '=':
          mCurrentToken->SetToken(ASSIGNEMENT_TOKEN);
          break;
        case 'a':
          AKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'b':
          BKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'c':
          CKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'd':
        case 'D':
          DKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'e':
          EKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'f':
          FKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'i':
          IKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'l':
          LKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'o':
          OKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'r':
          RKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 's':
          SKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 't':
          TKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'u':
          UKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'v':
          VKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'w':
          WKeywords(mTokenName + 1, mCurrentToken); 
          break;
        case 'x':
          XKeywords(mTokenName + 1, mCurrentToken);
          break;
        case '"':
          String(c, mCurrentToken); // has to be a string
          break;
        case '\'':
          Char(c, mCurrentToken); // has to be a char
          break;
        default:
          if (isalpha(c) || c == '_') {
            Identifier(mTokenName + 1, mCurrentToken); // has to be an identifier
          }
          else if (isdigit(c) || c == '-' || c == '.' || c == '+') {
            Number(c, mCurrentToken); // has to be some sort of number
          }
          else {
            // what to do? throw and exception?
            //throw ScannerUnknownCharacter("Unknow character");
            mCurrentToken->SetToken(ERROR_TOKEN);
          }
      } // switch
    }
    else {
      mCurrentToken->SetToken(EOF_TOKEN);
    }
  }

  return mCurrentToken;
}

void IdlScanner::SetCurrentToken()
{
  if (mCurrentToken) {
    if (mCurrentToken == mToken1) {
      mCurrentToken = mToken2;
      return;
    }
  }

  mCurrentToken = mToken1;
}

//
// Move to the first usable character and return it.
// Keep the line number up to date.
//
int IdlScanner::EatWhiteSpace()
{
  int c;
  c = mInputFile->get();
  while (EOF != c) {
    if (isspace(c)) {
      // increment line counter
      if (c == '\n')
        mLineNumber++;

      c = mInputFile->get();
    }
    else {
      break;
    }
  }

  return c;
}

//
// 'attribute' is the only keyword starting with 'a'.
// If that is not it, it must be an identifier
//
void IdlScanner::AKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'r' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'b' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'u' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'e' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(ATTRIBUTE_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(ATTRIBUTE_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'boolean' is the only keyword starting with 'b'.
// If that is not it, it must be an identifier
//
void IdlScanner::BKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'o'  && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'o'  && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'l'  && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'e'  && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'a'  && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'n'  && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(BOOLEAN_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(BOOLEAN_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// It's either 'char' or 'const' if it's a keyword
// otherwise must be an identifier
//
void IdlScanner::CKeywords(char *aCurrentPos, Token *aToken)
{
  char *startPos = aCurrentPos;
  int c = mInputFile->get();
  if (c != EOF && c == 'h' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'a' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'r' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(CHAR_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(CHAR_TOKEN);
    }
  }
  else if (aCurrentPos == startPos &&
            c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 's' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 't' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(CONST_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(CONST_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'double' and 'DOMString' are the only keyword starting with 'd'.
// If that is not it, it must be an identifier
//
void IdlScanner::DKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'u' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'b' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'l' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'e' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(DOUBLE_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(DOUBLE_TOKEN);
    }
  }
  else  if (c != EOF && c == 'O' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'M' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'S' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'r' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'g' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(STRING_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(STRING_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// It's either 'enum' or 'exception' if it's a keyword
// otherwise must be an identifier
//
void IdlScanner::EKeywords(char *aCurrentPos, Token *aToken)
{
  char *startPos = aCurrentPos;
  int c = mInputFile->get();
  if (c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'u' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'm' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(ENUM_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(ENUM_TOKEN);
    }
  }
  else if (aCurrentPos == startPos &&
            c != EOF && c == 'x' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'c' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'e' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'p' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'n' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(EXCEPTION_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(EXCEPTION_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'float' is the only keyword starting with 'f'.
// If that is not it, it must be an identifier
//
void IdlScanner::FKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'l' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'a' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 't' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(FLOAT_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(FLOAT_TOKEN);
    }
  }
  else if (c != EOF && c == 'u' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
           c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
           c != EOF && c == 'c' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
           c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
           c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
           c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
           c != EOF && c == 'n' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(FUNC_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(FUNC_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// One among - 'in' 'inout' 'int' 'interface' - or an identifier
//
void IdlScanner::IKeywords(char *aCurrentPos, Token *aToken)
{
  enum {
    INITIAL_STATE = 0,
    INOUT_O_STATE,
    INOUT_U_STATE,
    INTERFACE_E_STATE,
    INTERFACE_R_STATE,
    INTERFACE_F_STATE,
    INTERFACE_A_STATE,
    INTERFACE_C_STATE,

    IN_KEYWORD = 100,
    INT_KEYWORD,
    INOUT_KEYWORD,
    INTERFACE_KEYWORD
  };
  int state = INITIAL_STATE; // initial state

  int c = mInputFile->get();
  while (c != EOF && isalpha(c)) {

    *aCurrentPos++ = c;
    switch (c) {

      case 'n':
        if (state == INITIAL_STATE) {
          state = IN_KEYWORD;
          aToken->SetToken(INPUT_PARAM_TOKEN);
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 't':
        if (state == IN_KEYWORD) {
          state = INT_KEYWORD;
          aToken->SetToken(INT_TOKEN);
        }
        else if (state == INOUT_U_STATE) {
          state = INOUT_KEYWORD;
          aToken->SetToken(INOUT_PARAM_TOKEN);
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'e':
        if (state == INT_KEYWORD) {
          state = INTERFACE_E_STATE;
        }
        else if (state == INTERFACE_C_STATE) {
          state = INTERFACE_KEYWORD;
          aToken->SetToken(INTERFACE_TOKEN);
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'r':
        if (state == INTERFACE_E_STATE) {
          state = INTERFACE_R_STATE;
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'f':
        if (state == INTERFACE_R_STATE) {
          state = INTERFACE_F_STATE;
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'a':
        if (state == INTERFACE_F_STATE) {
          state = INTERFACE_A_STATE;
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'c':
        if (state == INTERFACE_A_STATE) {
          state = INTERFACE_C_STATE;
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'o':
        if (state == IN_KEYWORD) {
          state = INOUT_O_STATE;
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      case 'u':
        if (state == IN_KEYWORD) {
          state = INOUT_U_STATE;
        }
        else {
          Identifier(aCurrentPos, aToken);
          return;
        }
        break;

      default:
        Identifier(aCurrentPos, aToken);
        return;
    }

    c = mInputFile->get();
  }

  if (c == EOF) {
    if (state < IN_KEYWORD) {
      // end of file, return the identifier
      aToken->SetToken(IDENTIFIER_TOKEN, mTokenName);
    }
  }
  // if it is a digit or an underscore the token is
  // not terminated yet
  else if (isdigit(c) || c == '_') {
    Identifier(aCurrentPos, aToken);
  }
  else {
    mInputFile->putback(c);
  }
}

//
// 'long' is the only keyword starting with 'l'.
// If that is not it, it must be an identifier
//
void IdlScanner::LKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'g' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(LONG_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(LONG_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'out' is the only keyword starting with 'o'.
// If that is not it, it must be an identifier
//
void IdlScanner::OKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'u' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 't' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(OUTPUT_PARAM_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(OUTPUT_PARAM_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// it's either 'readonly' or 'raises' if it is a keyword.
// Otherwise, it must be an identifier
//
void IdlScanner::RKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'e' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'a' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'd' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'l' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'y' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(READONLY_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(READONLY_TOKEN);
    }
  }
  else if (c != EOF && c == 'a' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 's' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'e' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 's' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(RAISES_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(RAISES_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// One among - 'short' 'string' 'struct' - or an identifier
//
void IdlScanner::SKeywords(char *aCurrentPos, Token *aToken)
{
  char *startPos = aCurrentPos;
  int c = mInputFile->get();
  // chaeck for 'tr' 
  if (c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'r' && (*aCurrentPos++ = c) && (c = mInputFile->get())) {

    if (c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
        c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
        c != EOF && c == 'g' && (*aCurrentPos++ = c)) {
      // if terminated is a keyword
      c = mInputFile->get();
      if (c != EOF) {
        if (isalpha(c) || isdigit(c) || c == '_') {
          // more characters, it must be an identifier
          *aCurrentPos++ = c;
          Identifier(aCurrentPos, aToken);
        }
        else {
          // it is a keyword
          aToken->SetToken(STRING_TOKEN);
          mInputFile->putback(c);
        }
      }
      else {
        aToken->SetToken(STRING_TOKEN);
      }
    }
    else if (aCurrentPos == startPos + 2 &&
              c != EOF && c == 'u' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 'c' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 't' && (*aCurrentPos++ = c)) {
      // if terminated is a keyword
      c = mInputFile->get();
      if (c != EOF) {
        if (isalpha(c) || isdigit(c) || c == '_') {
          // more characters, it must be an identifier
          *aCurrentPos++ = c;
          Identifier(aCurrentPos, aToken);
        }
        else {
          // it is a keyword
          aToken->SetToken(STRUCT_TOKEN);
          mInputFile->putback(c);
        }
      }
      else {
        aToken->SetToken(STRUCT_TOKEN);
      }
    }
    else {
      // it must be an identifier
      KeywordMismatch(c, aCurrentPos, aToken);
    }
  }
  else if (aCurrentPos == startPos &&
            c != EOF && c == 'h' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 'r' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
            c != EOF && c == 't' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(SHORT_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(SHORT_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'typedef' is the only keyword starting with 't'.
// If that is not it, it must be an identifier
//
void IdlScanner::TKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'y' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'p' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'e' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'd' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'e' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'f' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(TYPEDEF_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(TYPEDEF_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// It's either 'union' or 'unsigned' if it's a keyword
// otherwise must be an identifier
//
void IdlScanner::UKeywords(char *aCurrentPos, Token *aToken)
{
  char *startPos = aCurrentPos;
  int c = mInputFile->get();
  // 'n' must follow
  if (c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get())) {

    if (c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
        c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
        c != EOF && c == 'n' && (*aCurrentPos++ = c)) {
      // if terminated is a keyword
      c = mInputFile->get();
      if (c != EOF) {
        if (isalpha(c) || isdigit(c) || c == '_') {
          // more characters, it must be an identifier
          *aCurrentPos++ = c;
          Identifier(aCurrentPos, aToken);
        }
        else {
          // it is a keyword
          aToken->SetToken(UNION_TOKEN);
          mInputFile->putback(c);
        }
      }
      else {
        aToken->SetToken(UNION_TOKEN);
      }
    }
    else if (aCurrentPos == startPos + 1 &&
              c != EOF && c == 's' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 'g' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 'e' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
              c != EOF && c == 'd' && (*aCurrentPos++ = c)) {
      // if terminated is a keyword
      c = mInputFile->get();
      if (c != EOF) {
        if (isalpha(c) || isdigit(c) || c == '_') {
          // more characters, it must be an identifier
          *aCurrentPos++ = c;
          Identifier(aCurrentPos, aToken);
        }
        else {
          // it is a keyword
          // find out what kind of "unsigned type" is
          SetCurrentToken(); // this is just to make sure next call uses the same token
          NextToken();
          if (SHORT_TOKEN == mCurrentToken->id) {
            aToken->SetToken(USHORT_TOKEN);
          }
          else if (INT_TOKEN == mCurrentToken->id) {
            aToken->SetToken(UINT_TOKEN);
          }
          else if (LONG_TOKEN == mCurrentToken->id) {
            aToken->SetToken(ULONG_TOKEN);
          }
        }
      }
    }
    else {
      // it must be an identifier
      KeywordMismatch(c, aCurrentPos, aToken);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'void' is the only keyword starting with 'v'.
// If that is not it, it must be an identifier
//
void IdlScanner::VKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'o' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'd' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(VOID_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(VOID_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// 'wstring' is the only keyword starting with 'w'.
// If that is not it, it must be an identifier
//
void IdlScanner::WKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 's' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 't' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'r' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'n' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'g' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(STRING_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(STRING_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

//
// Either 'xpidl' or an identifier
//
void IdlScanner::XKeywords(char *aCurrentPos, Token *aToken)
{
  int c = mInputFile->get();
  if (c != EOF && c == 'p' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'i' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'd' && (*aCurrentPos++ = c) && (c = mInputFile->get()) &&
      c != EOF && c == 'l' && (*aCurrentPos++ = c)) {
    // if terminated is a keyword
    c = mInputFile->get();
    if (c != EOF) {
      if (isalpha(c) || isdigit(c) || c == '_') {
        // more characters, it must be an identifier
        *aCurrentPos++ = c;
        Identifier(aCurrentPos, aToken);
      }
      else {
        // it is a keyword
        aToken->SetToken(XPIDL_TOKEN);
        mInputFile->putback(c);
      }
    }
    else {
      aToken->SetToken(XPIDL_TOKEN);
    }
  }
  else {
    // it must be an identifier
    KeywordMismatch(c, aCurrentPos, aToken);
  }
}

void IdlScanner::Identifier(char *aCurrentPos, Token *aToken)
{
  int index = aCurrentPos - mTokenName;
  int c = mInputFile->get();
  
  while (isalpha(c) || isdigit(c) || c == '_') {
    if (index > 254) {
      //XXX throw exception?
    }
    else {
      mTokenName[index++] = c;
    }
    c = mInputFile->get();
  }

  if (c != EOF) {
    mInputFile->putback(c);
  }

  aToken->SetToken(IDENTIFIER_TOKEN, mTokenName);
}

void IdlScanner::Number(int aStartChar, Token *aToken)
{
  long sign = 1;
  long value = 0;

  if (aStartChar == '-') {
    sign = -1;
    aStartChar = mInputFile->get();
  }
  else if (aStartChar == '.') {
    // double. Deal with it later
    aStartChar = mInputFile->get();
  }

  if (isdigit(aStartChar)) {
    long base = 10;
    if ('0' == aStartChar) {
      aStartChar = mInputFile->get();
      if (('x' == aStartChar) || ('X' == aStartChar)) {
        base = 16;
        aStartChar = mInputFile->get();
      }
      else if (isdigit(aStartChar)) {
        base = 8;
      }
      else {
        mInputFile->putback(aStartChar);
        aStartChar = '0';
      }
    }

    do {
      long digit;
      if (isdigit(aStartChar)) {
        digit = aStartChar - '0';
      }
      else if ((aStartChar >= 'a') && (aStartChar <= 'f')) {
        digit = 10 + (aStartChar - 'a');
      }
      else if ((aStartChar >= 'A') && (aStartChar <= 'F')) {
        digit = 10 + (aStartChar - 'A');
      }

      value = value * base + digit;
      aStartChar = mInputFile->get();
    } while (isdigit(aStartChar) || ((aStartChar >= 'a') && (aStartChar <= 'f')) || ((aStartChar >= 'A') && (aStartChar <= 'F')));

    if (aStartChar == '.') {
      // double. Deal with it later
    }
    else {
      mInputFile->putback(aStartChar);
    }

    aToken->SetTokenValue(INTEGER_CONSTANT, value * sign);
  }
}

void IdlScanner::String(int aStartChar, Token *aToken)
{
}

void IdlScanner::Char(int aStartChar, Token *aToken)
{
}

#define DEFAULT_COMMENT_SIZE    512
static char *kOptionalStr = "/* optional ";
static char *kEllipsisStr = "/* ... ";
static char *kIIDStr = "/* IID:";

void IdlScanner::Comment(char *aCurrentPos, Token *aToken)
{
  int bufferSize = DEFAULT_COMMENT_SIZE;
  char *aCommentBuffer = new char[bufferSize];
  aCommentBuffer[0] = '/'; // that's why we are here
  aCommentBuffer[1] = mInputFile->get(); 

  // c++ comment style
  if (aCommentBuffer[1] == '/') {
    aCommentBuffer[2] = mInputFile->get();
    char *pos = aCommentBuffer + 2; // point to the current char
    // untill end of line
    while (*pos++ != '\n') {
      if (pos - aCommentBuffer == bufferSize) {
        // grow the size of the buffer
        *pos = '\0';
        bufferSize *= 2;
        char *newBuffer = new char[bufferSize];
        strcpy(newBuffer, aCommentBuffer);
        delete[] aCommentBuffer;
        aCommentBuffer = newBuffer;
        pos = aCommentBuffer + bufferSize / 2;
      }
      *pos = mInputFile->get();
    }
    // go back one and terminate the string
    *--pos = '\0';
    // increment line count
    mLineNumber++;

    aToken->SetToken(COMMENT_TOKEN, aCommentBuffer);
  }
  // c comment style
  else if (aCommentBuffer[1] == '*') {
    aCommentBuffer[2] = mInputFile->get();
    aCommentBuffer[3] = mInputFile->get();
    char *pos = aCommentBuffer + 3; // point to the current char
    // untill end of comment
    while (*(pos - 1) != '*' && *pos++ != '/') {
      if (pos - aCommentBuffer == bufferSize) {
        // grow the size of the buffer
        *pos = '\0';
        bufferSize *= 2;
        char *newBuffer = new char[bufferSize];
        strcpy(newBuffer, aCommentBuffer);
        delete[] aCommentBuffer;
        aCommentBuffer = newBuffer;
        pos = aCommentBuffer + bufferSize / 2;
      }
      *pos = mInputFile->get();
      // increment line counter
      if (*pos == '\n')
        mLineNumber++;
    }
    // go back one and terminate the string
    *--pos = '\0';

    if (strcmp(aCommentBuffer, kOptionalStr) == 0) {
      aToken->SetToken(OPTIONAL_TOKEN, aCommentBuffer);
    }
    else if (strcmp(aCommentBuffer, kEllipsisStr) == 0) {
      aToken->SetToken(ELLIPSIS_TOKEN, aCommentBuffer);
    }
    else if (strncmp(aCommentBuffer, kIIDStr, strlen(kIIDStr)) == 0) {
      aToken->SetToken(IID_TOKEN, aCommentBuffer + strlen(kIIDStr));
    }
    else {
      aToken->SetToken(COMMENT_TOKEN, aCommentBuffer);
    }
  }
  else {
    // don't deal with it for now. It will report an error
  }

  delete[] aCommentBuffer;
}

void IdlScanner::KeywordMismatch(int aChar, char *aCurrentPos, Token *aToken)
{
  if (aChar != EOF) {
    // we did not read a keyword, must be an identifier
    if (isalpha(aChar) || isdigit(aChar) || aChar == '_') {
      // finish reading the identifier
      *aCurrentPos++ = aChar;
      Identifier(aCurrentPos, aToken);
    }
    else {
      // it's some terminator character, return the identifier
      aToken->SetToken(IDENTIFIER_TOKEN, mTokenName);
      mInputFile->putback(aChar);
    }
  }
  else {
    // end of file, return the identifier
    aToken->SetToken(IDENTIFIER_TOKEN, mTokenName);
  }
}

