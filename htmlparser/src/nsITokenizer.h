/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
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

#ifndef __NSITOKENIZER__
#define __NSITOKENIZER__

#include "nsISupports.h"
#include "prtypes.h"
#include "nshtmlpars.h"

class CToken;
class nsScanner;
class nsDeque;

#define NS_ITOKENIZER_IID      \
  {0xe4238ddc, 0x9eb6,  0x11d2, {0xba, 0xa5, 0x0,     0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


/***************************************************************
  Notes: 
 ***************************************************************/

class  nsITokenRecycler {
public:
    virtual void RecycleToken(CToken* aToken)=0;
};

/**
 * This interface is used as a callback to objects interested
 * in observing the token stream created from the parse process.
 */
class nsITokenObserver {
public:
  virtual PRBool  operator()(CToken* aToken)=0;
};

/***************************************************************
  Notes: 
 ***************************************************************/

class nsITokenizer : public nsISupports {
public:

  virtual nsresult          ConsumeToken(nsScanner& aScanner)=0;
  virtual nsITokenRecycler* GetTokenRecycler(void)=0;

  virtual CToken*           PushTokenFront(CToken* aToken)=0;
  virtual CToken*           PushToken(CToken* aToken)=0;
	virtual CToken*           PopToken(void)=0;
	virtual CToken*           PeekToken(void)=0;
	virtual PRInt32           GetCount(void)=0;
	virtual CToken*           GetTokenAt(PRInt32 anIndex)=0;

  virtual void              PrependTokens(nsDeque& aDeque)=0;
};


#endif

