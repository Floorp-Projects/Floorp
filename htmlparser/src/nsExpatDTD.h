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

#ifndef __NS_EXPAT_DTD
#define __NS_EXPAT_DTD

#include "nsIDTD.h"
#include "nsITokenizer.h"
#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"
#include "nsVoidArray.h"
#include "nsDeque.h"
#include "nsIContentSink.h"
#include "xmlparse.h"


#define NS_EXPAT_DTD_IID      \
  {0x5ad4b431, 0xcabb, 0x11d2, \
  {0xab, 0xcb, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}


class nsIDTDDebug;
class nsIParserNode;
class CITokenHandler;
class nsParser;



class nsExpatDTD : public nsIDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    nsExpatDTD();

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~nsExpatDTD();

    virtual const nsIID&  GetMostDerivedIID(void) const;

    /**
     * Call this method if you want the DTD to construct a clone of itself.
     * @update	gess7/23/98
     * @param 
     * @return
     */
    virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);

    /**
     * This method is called to determine if the given DTD can parse
     * a document in a given source-type. 
     * NOTE: Parsing always assumes that the end result will involve
     *       storing the result in the main content model.
     * @update	gess6/24/98
     * @param   
     * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
     */
    virtual eAutoDetectResult CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	gess5/18/98
      * @param	aFilename is the name of the file being parsed.
      * @return	error code (almost always 0)
      */
    NS_IMETHOD WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsString& aString,nsIContentSink* aSink=0);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	gess5/18/98
      * @param	aFilename is the name of the file being parsed.
      * @return	error code (almost always 0)
      */
    NS_IMETHOD BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver=0,nsIContentSink* aSink=0);

   /**
     * The parser uses a code sandwich to wrap the parsing process. Before
     * the process begins, WillBuildModel() is called. Afterwards the parser
     * calls DidBuildModel(). 
     * @update	gess5/18/98
     * @param	anErrorCode contans the last error that occured
     * @return	error code
     */
    NS_IMETHOD DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink=0);

    /**
     *  
     *  @update  gess 3/25/98
     *  @param   aToken -- token object to be put into content model
     *  @return  0 if all is well; non-zero is an error
     */
    NS_IMETHOD HandleToken(CToken* aToken,nsIParser* aParser);

    /**
     *  This method causes all tokens to be dispatched to the given tag handler.
     *
     *  @update  gess 3/25/98
  	 *  @param   aHandler -- object to receive subsequent tokens...
	   *  @return	 error code (usually 0)
     */
    NS_IMETHOD CaptureTokenPump(nsITagHandler* aHandler);

    /**
     *  This method releases the token-pump capture obtained in CaptureTokenPump()
     *
     *  @update  gess 3/25/98
  	 *  @param   aHandler -- object that received tokens...
	   *  @return	 error code (usually 0)
     */
    NS_IMETHOD ReleaseTokenPump(nsITagHandler* aHandler);

    /**
     * 
     * @update	gess12/28/98
     * @param 
     * @return
     */
    nsITokenizer* GetTokenizer(void);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillResumeParse(void);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    NS_IMETHOD WillInterruptParse(void);

    /**
     * Called by the parser to initiate dtd verification of the
     * internal context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual PRBool Verify(nsString& aURLRef,nsIParser* aParser);

    /**
     * Set this to TRUE if you want the DTD to verify its
     * context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual void SetVerification(PRBool aEnable);

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild) const;

    /**
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool IsContainer(PRInt32 aTag) const;

    /**
     * Retrieve a ptr to the global token recycler...
     * @update	gess8/4/98
     * @return  ptr to recycler (or null)
     */
    virtual nsITokenRecycler* GetTokenRecycler(void);

    /**
     * Parse an XML buffer using expat
     * @update	nra 2/29/99
     * @return  NS_ERROR_FAILURE if expat encounters an error, else NS_OK
     */
    nsresult ParseXMLBuffer(const char *buffer);
    
protected:
    /**
     * Sets up the callbacks for the expat parser      
     * @update  nra 2/24/99
     * @param   none
     * @return  none
     */
    void SetupExpatCallbacks(void);

    /* The callback handlers that get called from the expat parser */
    static void HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
    static void HandleEndElement(void *userData, const XML_Char *name);
    static void HandleCharacterData(void *userData, const XML_Char *s, int len);
    static void HandleProcessingInstruction(void *userData, 
      const XML_Char *target, 
      const XML_Char *data);
    static void HandleDefault(void *userData, const XML_Char *s, int len);
    static void HandleUnparsedEntityDecl(void *userData, 
      const XML_Char *entityName, 
      const XML_Char *base, 
      const XML_Char *systemId, 
      const XML_Char *publicId,
      const XML_Char *notationName);
    static void HandleNotationDecl(void *userData,
      const XML_Char *notationName,
      const XML_Char *base,
      const XML_Char *systemId,
      const XML_Char *publicId);
    static void HandleExternalEntityRef(XML_Parser parser,
      const XML_Char *openEntityNames,
      const XML_Char *base,
      const XML_Char *systemId,
      const XML_Char *publicId);
    static void HandleUnknownEncoding(void *encodingHandlerData,
      const XML_Char *name,
      XML_Encoding *info);
  
    XML_Parser          mExpatParser;
    nsParser*           mParser;
    nsIContentSink*     mSink;
    nsString            mFilename;
    PRInt32             mLineNumber;
    nsITokenizer*       mTokenizer;
};

extern NS_HTMLPARS nsresult NS_New_Expat_DTD(nsIDTD** aInstancePtrResult);


#endif
