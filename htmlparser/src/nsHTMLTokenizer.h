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

#ifndef __NSHTMLTOKENIZER
#define __NSHTMLTOKENIZER

#include "nsISupports.h"
#include "nsITokenizer.h"
#include "nsIDTD.h"
#include "prtypes.h"
#include "nsDeque.h"
#include "nsScanner.h"
#include "nsHTMLTokens.h"
#include "nsDTDUtils.h"

#define NS_HTMLTOKENIZER_IID      \
  {0xe4238ddd, 0x9eb6, 0x11d2, \
  {0xba, 0xa5, 0x0,     0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS nsHTMLTokenizer : public nsITokenizer {
public:
          nsHTMLTokenizer();
  virtual ~nsHTMLTokenizer();

          NS_DECL_ISUPPORTS

  virtual nsresult          ConsumeToken(nsScanner& aScanner);
  virtual nsITokenRecycler* GetTokenRecycler(void);

  virtual CToken*           PushTokenFront(CToken* theToken);
  virtual CToken*           PushToken(CToken* theToken);
	virtual CToken*           PopToken(void);
	virtual CToken*           PeekToken(void);
	virtual CToken*           GetTokenAt(PRInt32 anIndex);
	virtual PRInt32           GetCount(void);

  virtual void              PrependTokens(nsDeque& aDeque);

protected:

  virtual nsresult HandleSkippedContent(nsScanner& aScanner,CToken*& aToken);
  virtual nsresult ConsumeTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeStartTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeEndTag(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeAttributes(PRUnichar aChar,CStartToken* aToken,nsScanner& aScanner);
  virtual nsresult ConsumeEntity(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeWhitespace(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeComment(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeNewline(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeText(const nsString& aString,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeContentToEndTag(PRUnichar aChar,eHTMLTags aChildTag,nsScanner& aScanner,CToken*& aToken);
  virtual nsresult ConsumeProcessingInstruction(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);

  static void AddToken(CToken*& aToken,nsresult aResult,nsDeque& aDeque,CTokenRecycler* aRecycler);

  nsDeque mTokenDeque;
  PRBool  mDoXMLEmptyTags;
};

extern NS_HTMLPARS nsresult NS_NewHTMLTokenizer(nsIDTD** aInstancePtrResult);

#endif


