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

#ifndef NS_HTMLPARSER__
#define NS_HTMLPARSER__

#include "nsIParser.h"
#include "nsDeque.h"
#include "nsHTMLTokens.h"
#include "nsParserNode.h"
#include "nsTokenHandler.h"
#include "nsParserTypes.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsITokenizerDelegate.h"


#define NS_IHTML_PARSER_IID      \
  {0x2ce606b0, 0xbee6,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class CTokenizer;
class IContentSink;
class nsIHTMLContentSink;
class nsIURL;
class nsIDTD;


class nsHTMLParser : public nsIParser, public nsIStreamListener {
            
  public:
friend class CTokenHandler;

    NS_DECL_ISUPPORTS


    /**
     * default constructor
     * @update	gess5/11/98
     */
    nsHTMLParser();


    /**
     * Destructor
     * @update	gess5/11/98
     */
    ~nsHTMLParser();

    /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink);

    /**
     * Cause parser to parse input from given URL in given mode
     * @update	gess5/11/98
     * @param   aURL is a descriptor for source document
     * @param   aMode is the desired parser mode (Nav, other, etc.)
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual PRInt32 Parse(nsIURL* aURL,PRBool aIncremental=PR_FALSE);

    /**
     * Cause parser to parse input from given file in given mode
     * @update	gess5/11/98
     * @param   aFilename is a path for file document
     * @param   aMode is the desired parser mode (Nav, other, etc.)
     * @return  TRUE if all went well -- FALSE otherwise
     */
    virtual PRInt32 Parse(const char* aFilename,PRBool aIncremental);

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
    virtual PRInt32 ResumeParse(PRInt32 anIteration);

    /**
     * Retrieve ptr to internal context vector stack
     * @update	gess5/11/98
     * @param   stack ptr to be set
     * @return  count of items on stack (may be 0)
     */
    virtual PRInt32 GetStack(PRInt32* aStackPtr);

    /**
     * Ask parser if a given container is open ANYWHERE on stack
     * @update	gess5/11/98
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    virtual PRBool HasOpenContainer(PRInt32 aContainer) const;


    /**
     * This method gets called when a start token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleStartToken(CToken* aToken);

    /**
     * This method gets called when a start token has been consumed, and
     * we want to use default start token handling behavior.
     * This method gets called automatically by handleStartToken.
     *
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @param   aChildTag is the tag-type of given token
     * @param   aNode is a node be updated with info from given token
     * @return  TRUE if the token was handled.
     */
    PRBool HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsCParserNode& aNode);

    /**
     * This method gets called when an end token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the end token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleEndToken(CToken* aToken);

    /**
     * This method gets called when an entity token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the entity token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleEntityToken(CToken* aToken);

    /**
     * This method gets called when a comment token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the comment token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleCommentToken(CToken* aToken);

    /**
     * This method gets called when a skipped-content token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the skipped-content token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleSkippedContentToken(CToken* aToken);

    /**
     * This method gets called when an attribute token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the attribute token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleAttributeToken(CToken* aToken);

    /**
     * This method gets called when a script token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the script token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleScriptToken(CToken* aToken);
    
    /**
     * This method gets called when a style token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the style token to be handled
     * @return  TRUE if the token was handled.
     */
    PRBool HandleStyleToken(CToken* aToken);

      //*********************************************
      // These methods are callback methods used by
      // net lib to let us know about our inputstream.
      //*********************************************
    NS_IMETHOD GetBindInfo(void);
    NS_IMETHOD OnProgress(PRInt32 Progress, PRInt32 ProgressMax, const char *msg);
    NS_IMETHOD OnStartBinding(void);
    NS_IMETHOD OnDataAvailable(nsIInputStream *pIStream, PRInt32 length);
    NS_IMETHOD OnStopBinding(PRInt32 status, const char *msg);

protected:

    /**
     * This method gets called when the tokens have been consumed, and it's time
     * to build the model via the content sink.
     * @update	gess5/11/98
     * @return  YES if model building went well -- NO otherwise.
     */
    PRBool IterateTokens();

    /**
     * Retrieves the tag type for the element on the context vector stack at given pos.
     * @update	gess5/11/98
     * @param   pos of item you want to retrieve
     * @return  tag type (may be unknown)
     */
    eHTMLTags NodeAt(PRInt32 aPos) const;

    /**
     * Retrieve the tag type of the topmost item on context vector stack
     * @update	gess5/11/98
     * @return  tag type (may be unknown)
     */
    eHTMLTags GetTopNode() const;

    /**
     * Retrieve the number of items on context vector stack
     * @update	gess5/11/98
     * @return  count of items on stack -- may be 0
     */
    PRInt32 GetStackPos() const;

    /**
     * Causes the parser to scan foward, collecting nearby (sequential)
     * attribute tokens into the given node.
     * @update	gess5/11/98
     * @param   node to store attributes
     * @return  number of attributes added to node.
     */
    PRInt32 CollectAttributes(nsCParserNode& aNode);

    /**
     * Causes the next skipped-content token (if any) to
     * be consumed by this node.
     * @update	gess5/11/98
     * @param   node to consume skipped-content
     * @return  number of skipped-content tokens consumed.
     */
    PRInt32 CollectSkippedContent(nsCParserNode& aNode);

    /**
     * Causes token handlers to be registered for this parser.
     * DO NOT CALL THIS! IT'S DEPRECATED!
     * @update	gess5/11/98
     */
    void InitializeDefaultTokenHandlers();

    /**
     * DEPRECATED
     * @update	gess5/11/98
     */
    CTokenHandler* GetTokenHandler(const nsString& aString) const;

    /**
     * DEPRECATED
     * @update	gess5/11/98
     */
    CTokenHandler* GetTokenHandler(eHTMLTokenTypes aType) const;

    /**
     * DEPRECATED
     * @update	gess5/11/98
     */
    CTokenHandler* AddTokenHandler(CTokenHandler* aHandler);

    /**
     * DEPRECATED
     * @update	gess5/11/98
     */
    nsHTMLParser& DeleteTokenHandlers(void);
  
    //*************************************************
    //these cover methods mimic the sink, and are used
    //by the parser to manage its context-stack.
    //*************************************************

    /**
     * This cover method opens the given node as a HTML item in 
     * content sink.
     * @update	gess5/11/98
     * @param   HTML (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRBool OpenHTML(const nsIParserNode& aNode);

    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    PRBool CloseHTML(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a head item in 
     * content sink.
     * @update	gess5/11/98
     * @param   HEAD (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRBool OpenHead(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink head to be closed
     * @update	gess5/11/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRBool CloseHead(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a body item in 
     * content sink.
     * @update	gess5/11/98
     * @param   BODY (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRBool OpenBody(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink body to be closed
     * @update	gess5/11/98
     * @param   aNode is the body node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRBool CloseBody(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a form item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FORM (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRBool OpenForm(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink form to be closed
     * @update	gess5/11/98
     * @param   aNode is the form node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRBool CloseForm(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a frameset item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FRAMESET (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRBool OpenFrameset(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink frameset to be closed
     * @update	gess5/11/98
     * @param   aNode is the frameeset node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRBool CloseFrameset(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a generic container in 
     * content sink.
     * @update	gess5/11/98
     * @param   generic container (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRBool OpenContainer(const nsIParserNode& aNode);

    /**
     * This cover method causes a generic containre in the content-sink to be closed
     * @update	gess5/11/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRBool CloseContainer(const nsIParserNode& aNode);
    
    /**
     * This cover method causes the topmost container to be closed in sink
     * @update	gess5/11/98
     * @return  TRUE if all went well.
     */
    PRBool CloseTopmostContainer();
    
    /**
     * Cause all containers down to topmost given tag to be closed
     * @update	gess5/11/98
     * @param   aTag is the tag at which auto-closure should stop (inclusive) 
     * @return  TRUE if all went well -- otherwise FALSE
     */
    PRBool CloseContainersTo(eHTMLTags aTag);

    /**
     * Cause all containers down to given position to be closed
     * @update	gess5/11/98
     * @param   anIndex is the stack pos at which auto-closure should stop (inclusive) 
     * @return  TRUE if all went well -- otherwise FALSE
     */
    PRBool CloseContainersTo(PRInt32 anIndex);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gess5/11/98
     * @param   aNode is leaf node to be added.
     * @return  TRUE if all went well -- FALSE otherwise.
     */
    PRBool AddLeaf(const nsIParserNode& aNode);

    /**
     * Determine if the given tag is open in the context vector stack.
     * @update	gess5/11/98
     * @param   aTag is the type of tag you want to test for openness
     * @return  TRUE if open, FALSE otherwise.
     */
    PRBool IsOpen(eHTMLTags aTag) const;

    /**
     * Finds the topmost occurance of given tag within context vector stack.
     * @update	gess5/11/98
     * @param   tag to be found
     * @return  index of topmost tag occurance -- may be -1 (kNotFound).
     */
    PRInt32 GetTopmostIndex(eHTMLTags aTag) const;

    /**
     * Causes auto-closures of context vector stack in order to find a 
     * proper home for the given child. Propagation may also occur as 
     * a fall out.
     * @update	gess5/11/98
     * @param   child to be added (somewhere) to context vector stack.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    PRBool ReduceContextStackFor(PRInt32 aChildTag);

    /**
     * Attempt forward and/or backward propagation for the given
     * child within the current context vector stack.
     * @update	gess5/11/98
     * @param   type of child to be propagated.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    PRBool CreateContextStackFor(PRInt32 aChildTag);

private:
    PRInt32 ParseFileIncrementally(const char* aFilename);  //XXX ONLY FOR DEBUG PURPOSES...

protected:
    //*********************************************
    // And now, some data members...
    //*********************************************

    nsIHTMLContentSink* mSink;
    CTokenizer*         mTokenizer;

    PRInt32             mContextStack[50];
    PRInt32             mContextStackPos;

    CTokenHandler*      mTokenHandlers[100];
    PRInt32             mTokenHandlerCount;
    nsDequeIterator*    mCurrentPos;

    nsIDTD*             mDTD;
    eParseMode          mParseMode;
    PRBool              mHasOpenForm;
    PRBool              mIncremental;
    ITokenizerDelegate* mDelegate;
};


#endif 



