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
#include "nsParserTypes.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"


#define NS_PARSER_IID      \
  {0x2ce606b0, 0xbee6,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class IContentSink;
class nsIHTMLContentSink;
class nsIURL;
class nsIDTD;
class nsIDTDDebug;
class CScanner;
class nsIParserFilter;


class nsParser : public nsIParser, public nsIStreamListener {
            
  public:
friend class CTokenHandler;

    NS_DECL_ISUPPORTS


    /**
     * default constructor
     * @update	gess5/11/98
     */
    nsParser();


    /**
     * Destructor
     * @update	gess5/11/98
     */
    ~nsParser();

    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink);
    
    virtual nsIParserFilter* SetParserFilter(nsIParserFilter* aFilter);

    virtual void SetDTD(nsIDTD* aDTD);
    
    virtual nsIDTD* GetDTD(void);
    
    /**
     *  
     *  
     *  @update  gess 6/9/98
     *  @param   
     *  @return  
     */
    virtual CScanner* GetScanner(void);

    /**
     * Cause parser to parse input from given URL in given mode
     * @update	gess5/11/98
     * @param   aURL is a descriptor for source document
     * @param   aListener is a listener to forward notifications to
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual PRInt32 Parse(nsIURL* aURL,
                          nsIStreamListener* aListener, nsIDTDDebug * aDTDDebug = 0);

    /**
     * Cause parser to parse input from given file in given mode
     * @update	gess5/11/98
     * @param   aFilename is a path for file document
     * @param   aMode is the desired parser mode (Nav, other, etc.)
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual PRInt32 Parse(const char* aFilename);

    /**
     * @update	gess5/11/98
     * @param   anHTMLString contains a string-full of real HTML
     * @param   appendTokens tells us whether we should insert tokens inline, or append them.
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual PRInt32 Parse(nsString& anHTMLString,PRBool appendTokens);

    /**
     * This method gets called (automatically) during incremental parsing
     * @update	gess5/11/98
     * @return  TRUE if all went well, otherwise FALSE
     */
    virtual PRInt32 ResumeParse(void);

    /**
     * Causes the parser to scan foward, collecting nearby (sequential)
     * attribute tokens into the given node.
     * @update	gess5/11/98
     * @param   node to store attributes
     * @return  number of attributes added to node.
     */
    virtual PRInt32 CollectAttributes(nsCParserNode& aNode,PRInt32 aCount);

    /**
     * Causes the next skipped-content token (if any) to
     * be consumed by this node.
     * @update	gess5/11/98
     * @param   node to consume skipped-content
     * @param   holds the number of skipped content elements encountered
     * @return  Error condition.
     */
    virtual PRInt32 CollectSkippedContent(nsCParserNode& aNode,PRInt32& aCount);

    /**
     *  This debug routine is used to cause the tokenizer to
     *  iterate its token list, asking each token to dump its
     *  contents to the given output stream.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  
     */
    void DebugDumpSource(ostream& out);


     //*********************************************
      // These methods are callback methods used by
      // net lib to let us know about our inputstream.
      //*********************************************
    NS_IMETHOD GetBindInfo(void);
    NS_IMETHOD OnProgress(PRInt32 Progress, PRInt32 ProgressMax, const nsString& aMmsg);
    NS_IMETHOD OnStartBinding(const char *aContentType);
    NS_IMETHOD OnDataAvailable(nsIInputStream *pIStream, PRInt32 length);
    NS_IMETHOD OnStopBinding(PRInt32 status, const nsString& aMsg);

protected:

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    PRInt32 WillBuildModel(const char* aFilename=0,const char* aContentType=0);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    PRInt32 DidBuildModel(PRInt32 anErrorCode);

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  YES if model building went well -- NO otherwise.
     */
    virtual PRInt32 IterateTokens(void);
  
private:

    /*******************************************
      These are the tokenization methods...
     *******************************************/

    /**
     *  Cause the tokenizer to consume the next token, and 
     *  return an error result.
     *  
     *  @update  gess 3/25/98
     *  @param   anError -- ref to error code
     *  @return  new token or null
     */
    virtual PRInt32 ConsumeToken(CToken*& aToken);

    /**
     *  Part of the code sandwich, this gets called right before
     *  the tokenization process begins. The main reason for
     *  this call is to allow the delegate to do initialization.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  TRUE if it's ok to proceed
     */
    PRBool WillTokenize(void);

    /**
     *  
     *  @update  gess 3/25/98
     *  @return  TRUE if it's ok to proceed
     */
    PRInt32 Tokenize(nsString& aSourceBuffer,PRBool appendTokens);

    /**
     *  This is the primary control routine. It iteratively
     *  consumes tokens until an error occurs or you run out
     *  of data.
     *  
     *  @update  gess 3/25/98
     *  @return  error code 
     */
    PRInt32 Tokenize(void);

    /**
     *  This is the tail-end of the code sandwich for the
     *  tokenization process. It gets called once tokenziation
     *  has completed.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  TRUE if all went well
     */
    PRBool DidTokenize(void);

    /**
     *  This debug routine is used to cause the tokenizer to
     *  iterate its token list, asking each token to dump its
     *  contents to the given output stream.
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return  
     */
    void DebugDumpTokens(ostream& out);

    
    /**
     * This method is used as a backstop to compute the kind of content
     * that is contained in the scanner stream. This method is important
     * because it allows us to defer the resolution of our DTD (and hence)
     * filters and maybe eventually sinks based on the input type.
     *
     * @update	gess6/22/98
     * @param 
     * @return  TRUE if we figured it out.
     */
    PRBool DetermineContentType(const char* aContentType);


protected:
    //*********************************************
    // And now, some data members...
    //*********************************************

    nsIStreamListener*  mListener;
    nsIContentSink*     mSink;
    nsIParserFilter*    mParserFilter;

    nsDequeIterator*    mCurrentPos;
    nsDequeIterator*    mMarkPos;

    nsIDTD*             mDTD;
    eParseMode          mParseMode;
    char*               mTransferBuffer;

    PRInt32             mMajorIteration;
    PRInt32             mMinorIteration;

    nsDeque             mTokenDeque;
    CScanner*           mScanner;
    nsIURL*             mURL;
	nsIDTDDebug*		mDTDDebug;
};


#endif 

