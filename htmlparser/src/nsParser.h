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
 *  This class does two primary jobs:
 *    1) It iterates the tokens provided during the 
 *       tokenization process, identifing where elements
 *       begin and end (doing validation and normalization).
 *    2) It controls and coordinates with an instance of
 *       the IContentSink interface, to coordinate the
 *       the production of the content model.
 *
 *  The basic operation of this class assumes that an HTML
 *  document is non-normalized. Therefore, we don't process
 *  the document in a normalized way. Don't bother to look
 *  for methods like: doHead() or doBody().
 *
 *  Instead, in order to be backward compatible, we must
 *  scan the set of tokens and perform this basic set of
 *  operations:
 *    1)  Determine the token type (easy, since the tokens know)
 *    2)  Determine the appropriate section of the HTML document
 *        each token belongs in (HTML,HEAD,BODY,FRAMESET).
 *    3)  Insert content into our document (via the sink) into
 *        the correct section.
 *    4)  In the case of tags that belong in the BODY, we must
 *        ensure that our underlying document state reflects
 *        the appropriate context for our tag. 
 *
 *        For example,if we see a <TR>, we must ensure our 
 *        document contains a table into which the row can
 *        be placed. This may result in "implicit containers" 
 *        created to ensure a well-formed document.
 *         
 */

#ifndef NS_PARSER__
#define NS_PARSER__

#include "nsIParser.h"
#include "nsDeque.h"
#include "nsParserNode.h"
#include "nsIURL.h"
#include "CParserContext.h"
#include "nsParserCIID.h"
#include "nsITokenizer.h"
#include "nsHTMLTags.h"

class IContentSink;
class nsIHTMLContentSink;
class nsIDTD;
class nsScanner;
class nsIParserFilter;
class nsTagStack;

#include <fstream.h>

#ifndef XP_MAC
#pragma warning( disable : 4275 )
#endif


CLASS_EXPORT_HTMLPARS nsParser : public nsIParser, public nsIStreamListener {
            
  public:
friend class CTokenHandler;

    NS_DECL_ISUPPORTS


    /**
     * default constructor
     * @update	gess5/11/98
     */
    nsParser(nsITokenObserver* anObserver=0);


    /**
     * Destructor
     * @update	gess5/11/98
     */
    virtual ~nsParser();

    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink);

    /**
     * retrive the sink set into the parser 
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* GetContentSink(void);
    
    /**
     *  Call this method once you've created a parser, and want to instruct it
	   *  about the command which caused the parser to be constructed. For example,
     *  this allows us to select a DTD which can do, say, view-source.
     *  
     *  @update  gess 3/25/98
     *  @param   aContentSink -- ptr to content sink that will receive output
     *  @return	 ptr to previously set contentsink (usually null)  
     */
    virtual void SetCommand(const char* aCommand);

    virtual nsIParserFilter* SetParserFilter(nsIParserFilter* aFilter);
    
    virtual void RegisterDTD(nsIDTD* aDTD);

    /**
     *  Retrieve the scanner from the topmost parser context
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    virtual eParseMode GetParseMode(void);

    /**
     *  Retrieve the scanner from the topmost parser context
     *  
     *  @update  gess 6/9/98
     *  @return  ptr to scanner
     */
    virtual nsScanner* GetScanner(void);

    /**
     * Cause parser to parse input from given URL 
     * @update	gess5/11/98
     * @param   aURL is a descriptor for source document
     * @param   aListener is a listener to forward notifications to
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual nsresult Parse(nsIURL* aURL,nsIStreamObserver* aListener,PRBool aEnableVerify=PR_FALSE);

    /**
     * Cause parser to parse input from given stream 
     * @update	gess5/11/98
     * @param   aStream is the i/o source
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual nsresult Parse(nsIInputStream& aStream,PRBool aEnableVerify=PR_FALSE);

    /**
     * @update	gess5/11/98
     * @param   anHTMLString contains a string-full of real HTML
     * @param   appendTokens tells us whether we should insert tokens inline, or append them.
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual nsresult Parse(nsString& aSourceBuffer,void* aKey,const nsString& aContentType,PRBool aEnableVerify=PR_FALSE,PRBool aLastCall=PR_FALSE);

    virtual PRBool IsValidFragment(nsString& aSourceBuffer,nsTagStack& aStack,nsHTMLTag aTag,const nsString& aContentType);
    virtual PRBool ParseFragment(nsString& aSourceBuffer,void* aKey,nsTagStack& aStack,nsHTMLTag aTag,const nsString& aContentType);


    /**
     *  Call this when you want control whether or not the parser will parse
     *  and tokenize input (TRUE), or whether it just caches input to be 
     *  parsed later (FALSE).
     *  
     *  @update  gess 9/1/98
     *  @param   aState determines whether we parse/tokenize or just cache.
     *  @return  current state
     */
    virtual PRBool EnableParser(PRBool aState);


    /**
     *  This rather arcane method (hack) is used as a signal between the
     *  DTD and the parser. It allows the DTD to tell the parser that content
     *  that comes through (parser::parser(string)) but not consumed should
     *  propagate into the next string based parse call.
     *  
     *  @update  gess 9/1/98
     *  @param   aState determines whether we propagate unused string content.
     *  @return  current state
     */
    void SetUnusedInput(nsString& aBuffer);

    /**
     * This method gets called (automatically) during incremental parsing
     * @update	gess5/11/98
     * @return  TRUE if all went well, otherwise FALSE
     */
    virtual nsresult ResumeParse(nsIDTD* mDefaultDTD=0);

    void  DebugDumpSource(ostream& anOutput);

     //*********************************************
      // These methods are callback methods used by
      // net lib to let us know about our inputstream.
      //*********************************************
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 Progress, PRUint32 ProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMmsg);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 length);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg);

    void              PushContext(CParserContext& aContext);
    CParserContext*   PopContext();
    CParserContext*   PeekContext() {return mParserContext;}

    /**
     * 
     * @update	gess 1/22/99
     * @param 
     * @return
     */
    virtual nsITokenizer* GetTokenizer(void);

	/*
	 * The following two should be removed once our Meta Charset work complete
	 */
	static nsString gHackMetaCharset; 
	static nsString gHackMetaCharsetURL;

protected:

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    nsresult WillBuildModel(nsString& aFilename,nsIDTD* mDefaultDTD=0);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    nsresult DidBuildModel(nsresult anErrorCode);

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  YES if model building went well -- NO otherwise.
     */
    virtual nsresult BuildModel(void);
    
private:

    /*******************************************
      These are the tokenization methods...
     *******************************************/

    /**
     *  Part of the code sandwich, this gets called right before
     *  the tokenization process begins. The main reason for
     *  this call is to allow the delegate to do initialization.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  TRUE if it's ok to proceed
     */
    PRBool WillTokenize();

   
    /**
     *  This is the primary control routine. It iteratively
     *  consumes tokens until an error occurs or you run out
     *  of data.
     *  
     *  @update  gess 3/25/98
     *  @return  error code 
     */
    nsresult Tokenize();

    /**
     *  This is the tail-end of the code sandwich for the
     *  tokenization process. It gets called once tokenziation
     *  has completed.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  TRUE if all went well
     */
    PRBool DidTokenize();
    

protected:
    //*********************************************
    // And now, some data members...
    //*********************************************
    
  
    CParserContext*     mParserContext;
    PRInt32             mMajorIteration;
    PRInt32             mMinorIteration;

    nsIStreamObserver*  mObserver;
    nsIContentSink*     mSink;
    nsIParserFilter*    mParserFilter;
    PRBool              mDTDVerification;
    nsString            mCommand;
    PRInt32             mStreamStatus;
    nsITokenObserver*   mTokenObserver;
    nsString            mUnusedInput;
};


#endif 

