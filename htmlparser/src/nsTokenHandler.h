/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef CTokenHandler__
#define CTokenHandler__

#include "prtypes.h"
#include "nsString.h"
#include "nsHTMLTokens.h"
#include "nsITokenHandler.h"

class CToken;
class nsHTMLParser;


class CTokenHandler : public CITokenHandler {
public:
                          CTokenHandler(eHTMLTokenTypes aType=eToken_unknown);
  virtual                 ~CTokenHandler();
                          
  virtual   eHTMLTokenTypes GetTokenType(void);
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);

protected:
            eHTMLTokenTypes mType;
};


class CStartTokenHandler : public CTokenHandler  {
public:
                          CStartTokenHandler();
  virtual                 ~CStartTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CEndTokenHandler : public CTokenHandler  {
public:
                          CEndTokenHandler();
  virtual                 ~CEndTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CCommentTokenHandler : public CTokenHandler  {
public:
                          CCommentTokenHandler();
  virtual                 ~CCommentTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CEntityTokenHandler : public CTokenHandler  {
public:
                          CEntityTokenHandler();
  virtual                 ~CEntityTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CWhitespaceTokenHandler : public CTokenHandler  {
public:
                          CWhitespaceTokenHandler();
  virtual                 ~CWhitespaceTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CNewlineTokenHandler : public CTokenHandler  {
public:
                          CNewlineTokenHandler();
  virtual                 ~CNewlineTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CTextTokenHandler : public CTokenHandler  {
public:
                          CTextTokenHandler();
  virtual                 ~CTextTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CAttributeTokenHandler : public CTokenHandler  {
public:
                          CAttributeTokenHandler();
  virtual                 ~CAttributeTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CScriptTokenHandler :  public CTokenHandler  {
public:
                          CScriptTokenHandler();
  virtual                 ~CScriptTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CStyleTokenHandler : public CTokenHandler  {
public:
                          CStyleTokenHandler();
  virtual                 ~CStyleTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


class CSkippedContentTokenHandler : public CTokenHandler  {
public:
                          CSkippedContentTokenHandler();
  virtual                 ~CSkippedContentTokenHandler();
                          
  virtual   PRBool        operator()(CToken* aToken,nsHTMLParser* aParser);
  virtual   PRBool        CanHandle(eHTMLTokenTypes aType);
};


#endif


