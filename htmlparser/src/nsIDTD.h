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

#ifndef NS_IDTD__
#define NS_IDTD__

#include "nsISupports.h"
#include "prtypes.h"

#define NS_IDTD_IID      \
  {0x75634940, 0xcfdc,  0x11d1,  \
  {0xaa, 0xda, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class nsIParser;
class CToken;
class nsIContentSink;
class nsIDTDDebug;
class nsIURL;

class nsIDTD : public nsISupports {
            
  public:

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual void SetParser(nsIParser* aParser)=0;

   /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink)=0;

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual PRInt32 WillBuildModel(const char* aFilename=0)=0;

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual PRInt32 DidBuildModel(PRInt32 anErrorCode)=0;
    
    /**
     *  
     *  @update  gess 3/25/98
     *  @param   aToken -- token object to be put into content model
     *  @return  0 if all is well; non-zero is an error
     */
    virtual PRInt32 HandleToken(CToken* aToken)=0;


    /**
     *  Cause the tokenizer to consume the next token, and 
     *  return an error result.
     *  
     *  @update  gess 3/25/98
     *  @param   anError -- ref to error code
     *  @return  new token or null
     */
    virtual PRInt32 ConsumeToken(CToken*& aToken)=0;


    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual void WillResumeParse(void)=0;

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual void WillInterruptParse(void)=0;

    /**
     * 
     * @update	jevering 6/18/98
     * @param  aParent  parent tag
     * @param  aChild   child tag
     * @return PR_TRUE if valid container
     */
    virtual PRBool CanContain(PRInt32 aParent, PRInt32 aChild) = 0;

    /**
     * 
     * @update	jevering6/23/98
     * @param 
     * @return
     */
	virtual void SetDTDDebug(nsIDTDDebug * aDTDDebug) = 0;
};


#endif 



