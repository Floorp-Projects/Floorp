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
 * LAST MODS:  gess 28Feb98
 * 
 * This file declares the basic tokenizer class. The 
 * central theme of this class is to control and 
 * coordinate a tokenization process. Note that this 
 * class is grammer-neutral: this class doesn't care
 * at all what the underlying stream consists of. 
 *
 * The main purpose of this class is to iterate over an
 * input stream with the help of a given scanner and a
 * given type-specific tokenizer-Delegate.
 *
 * The primary method here is the tokenize() method, which
 * simple loops calling getToken() until an EOF condition
 * (or some other error) occurs.
 *
 */


#ifndef  TOKENIZER
#define  TOKENIZER

#include "nsToken.h"
#include "nsITokenizerDelegate.h"
#include "nsDeque.h"
#include <iostream.h>

class CScanner;
class nsIURL;

class  CTokenizer {
  public:
                        CTokenizer(nsIURL* aURL,ITokenizerDelegate* aDelegate,eParseMode aMode);
                        ~CTokenizer();
    
    PRInt32             Tokenize(void);
    CToken*              GetToken(PRInt32& anErrorCode);
    PRInt32             GetSize(void);
    nsDeque&            GetDeque(void);

    void                DebugDumpSource(ostream& out);
    void                DebugDumpTokens(ostream& out);
    static void         SelfTest();

  protected:  

    PRBool              WillTokenize();
    PRBool              DidTokenize();

    ITokenizerDelegate*  mDelegate;
    CScanner*           mScanner;
    nsDeque             mTokenDeque;
    eParseMode          mParseMode;
};

#endif


