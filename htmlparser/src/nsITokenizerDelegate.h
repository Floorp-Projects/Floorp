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
 * This virtual base class is used to define the basic
 * tokenizer delegate interface. As you can see, it is
 * very simple.
 *
 * The only routines we use at this point are getToken() 
 * and willAddToken(). While getToken() is obvious, 
 * willAddToken() may not be. The purpose of the method
 * is to allow the delegate to decide whether or not a 
 * given token that was just read in the tokenization process
 * should be included in the total list of tokens. This
 * method gives you a chance to say, "No, ignore this token".
 *
 */

#ifndef  ITOKENIZERDELEGATE
#define  ITOKENIZERDELEGATE

#include "prtypes.h"
#include "nsParserTypes.h"
#include "nsIDTD.h"

class CScanner;
class CToken;

class ITokenizerDelegate {
  public:

      virtual PRBool      WillTokenize()=0;
      virtual PRBool      DidTokenize()=0;

      virtual CToken*     GetToken(CScanner& aScanner,PRInt32& anErrorCode)=0;
      virtual PRBool      WillAddToken(CToken& aToken)=0;

      virtual eParseMode  GetParseMode(void) const=0;
      virtual nsIDTD*     GetDTD(void) const=0;
};

#endif
