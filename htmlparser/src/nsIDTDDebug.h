/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef NS_IDTDDEBUG__
#define NS_IDTDDEBUG__

#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "prtypes.h"

#define NS_IDTDDEBUG_IID      \
  {0x7b68c220, 0x0685,  0x11d2,  \
  {0xa4, 0xb5, 0x00,    0x80, 0x5f, 0x2a, 0x0e, 0xd2}}


class nsIDTD;
class nsIParser;
class nsVoidArray;
// enum eHTMLTags;

class nsIDTDDebug : public nsISupports {
            
public:

   virtual void SetVerificationDirectory(char * verify_dir) = 0;

   virtual void SetRecordStatistics(PRBool bval) = 0;

   virtual PRBool Verify(nsIDTD * aDTD, nsIParser * aParser, int ContextStackPos, eHTMLTags aContextStack[], nsString& aURLRef) = 0;

   virtual void DumpVectorRecord(void) = 0;

};

extern NS_EXPORT nsresult NS_NewDTDDebug(nsIDTDDebug** aInstancePtrResult);

#endif /* NS_IDTDDEBUG__ */
