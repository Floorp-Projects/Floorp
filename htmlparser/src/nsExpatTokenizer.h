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

#ifndef __nsExpatTokenizer
#define __nsExpatTokenizer

#include "nsISupports.h"
#include "nsHTMLTokenizer.h"
#include "prtypes.h"

#define NS_EXPATTOKENIZER_IID      \
  {0x483836aa, 0xcabe, 0x11d2, { 0xab, 0xcb, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS nsExpatTokenizer : public nsHTMLTokenizer {
public:
            nsExpatTokenizer();
  virtual   ~nsExpatTokenizer();

            NS_DECL_ISUPPORTS

  virtual nsresult ConsumeToken(nsScanner& aScanner);
};

extern NS_HTMLPARS nsresult NS_Expat_Tokenizer(nsIDTD** aInstancePtrResult);

#endif


