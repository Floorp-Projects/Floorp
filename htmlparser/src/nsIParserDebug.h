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
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef NS_IPARSERDEBUG__
#define NS_IPARSERDEBUG__

#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "prtypes.h"

#define NS_IPARSERDEBUG_IID      \
  {0x7b68c220, 0x0685,  0x11d2,  \
  {0xa4, 0xb5, 0x00,    0x80, 0x5f, 0x2a, 0x0e, 0xd2}}


class nsIDTD;
class nsParser;

class nsIParserDebug : public nsISupports {
            
public:

   virtual void SetVerificationDirectory(char * verify_dir) = 0;

   virtual void SetRecordStatistics(PRBool bval) = 0;

   virtual PRBool Verify(nsIDTD * aDTD, nsParser * aParser, int ContextStackPos, eHTMLTags aContextStack[], char * aURLRef) = 0;

   virtual void DumpVectorRecord(void) = 0;

};

extern NS_EXPORT nsresult NS_NewParserDebug(nsIParserDebug** aInstancePtrResult);

#endif /* NS_IPARSERDEBUG__ */