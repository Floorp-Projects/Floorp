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

#ifndef NS_OTHERHTMLDTD__
#define NS_OTHERHTMLDTD__

#include "CNavDTD.h"


#define NS_IOtherHTML_DTD_IID      \
  {0x8a5e89c0, 0xd16d,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x8, 0x14, 0x98, 0x89}}


//class nsParser;
//class nsIHTMLContentSink;

class COtherDTD : public CNavDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    COtherDTD();

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~COtherDTD();

    virtual PRBool IsCapableOf(eProcessType aProcessType, nsString& aString,PRInt32 aVersion);
    
    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual PRInt32 WillBuildModel(const char* aFilename=0);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual PRInt32 DidBuildModel(PRInt32 anErrorCode);

    /**
     *  
     *  @update  gess 3/25/98
     *  @param   aToken -- token object to be put into content model
     *  @return  0 if all is well; non-zero is an error
     */
    virtual PRInt32 HandleToken(CToken* aToken);

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
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual void WillResumeParse(void);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual void WillInterruptParse(void);

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild);

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContainIndirect(eHTMLTags aParent,eHTMLTags aChild) const;

    /**
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool CanOmit(eHTMLTags aParent,eHTMLTags aChild)const;

    /**
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag type of parent
     *  @param   aChild -- tag type of child
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool CanOmitEndTag(eHTMLTags aParent,eHTMLTags aChild)const;

    /**
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool IsContainer(eHTMLTags aTags) const;

    /**
     * This method does two things: 1st, help construct
     * our own internal model of the content-stack; and
     * 2nd, pass this message on to the sink.
     * @update  gess4/6/98
     * @param   aNode -- next node to be added to model
     * @return  TRUE if ok, FALSE if error
     */
    virtual eHTMLTags GetDefaultParentTagFor(eHTMLTags aTag) const;

    /**
     * This method tries to design a context map (without actually
     * changing our parser state) from the parent down to the
     * child. 
     *
     * @update  gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  True if closure was achieved -- other false
     */
    virtual PRBool ForwardPropagate(nsString& aVector,eHTMLTags aParentTag,eHTMLTags aChildTag);

    /**
     * This method tries to design a context map (without actually
     * changing our parser state) from the child up to the parent.
     *
     * @update  gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  True if closure was achieved -- other false
     */
    virtual PRBool BackwardPropagate(nsString& aVector,eHTMLTags aParentTag,eHTMLTags aChildTag) const;

    /**
     * 
     * @update	gess6/4/98
     * @param 
     * @return
     */
    virtual PRInt32 DidOpenContainer(eHTMLTags aTag,PRBool anExplicitOpen);    

    /**
     * 
     * @update	gess6/4/98
     * @param 
     * @return
     */
    virtual PRInt32 DidCloseContainer(eHTMLTags aTag,PRBool anExplicitClosure);

    /**
     * This method gets called when a start token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleStartToken(CToken* aToken);

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
    PRInt32 HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode);

    /**
     * This method gets called when an end token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the end token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleEndToken(CToken* aToken);

    /**
     * This method gets called when an entity token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the entity token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleEntityToken(CToken* aToken);

    /**
     * This method gets called when a comment token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the comment token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleCommentToken(CToken* aToken);

    /**
     * This method gets called when a skipped-content token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the skipped-content token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleSkippedContentToken(CToken* aToken);

    /**
     * This method gets called when an attribute token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the attribute token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleAttributeToken(CToken* aToken);

    /**
     * This method gets called when a script token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the script token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleScriptToken(CToken* aToken);
    
    /**
     * This method gets called when a style token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the style token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleStyleToken(CToken* aToken);

private:


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
    PRInt32 OpenHTML(const nsIParserNode& aNode);

    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    PRInt32 CloseHTML(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a head item in 
     * content sink.
     * @update	gess5/11/98
     * @param   HEAD (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenHead(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink head to be closed
     * @update	gess5/11/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRInt32 CloseHead(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a body item in 
     * content sink.
     * @update	gess5/11/98
     * @param   BODY (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenBody(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink body to be closed
     * @update	gess5/11/98
     * @param   aNode is the body node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRInt32 CloseBody(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a form item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FORM (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenForm(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink form to be closed
     * @update	gess5/11/98
     * @param   aNode is the form node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRInt32 CloseForm(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a form item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FORM (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenMap(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink form to be closed
     * @update	gess5/11/98
     * @param   aNode is the form node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRInt32 CloseMap(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a frameset item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FRAMESET (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenFrameset(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink frameset to be closed
     * @update	gess5/11/98
     * @param   aNode is the frameeset node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRInt32 CloseFrameset(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a generic container in 
     * content sink.
     * @update	gess5/11/98
     * @param   generic container (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack);

    /**
     * This cover method causes a generic containre in the content-sink to be closed
     * @update	gess5/11/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    PRInt32 CloseContainer(const nsIParserNode& aNode,eHTMLTags anActualTag,PRBool aUpdateStyles);
    
    /**
     * This cover method causes the topmost container to be closed in sink
     * @update	gess5/11/98
     * @return  TRUE if all went well.
     */
    PRInt32 CloseTopmostContainer();
    
    /**
     * Cause all containers down to topmost given tag to be closed
     * @update	gess5/11/98
     * @param   aTag is the tag at which auto-closure should stop (inclusive) 
     * @return  TRUE if all went well -- otherwise FALSE
     */
    PRInt32 CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles);

    /**
     * Cause all containers down to given position to be closed
     * @update	gess5/11/98
     * @param   anIndex is the stack pos at which auto-closure should stop (inclusive) 
     * @return  TRUE if all went well -- otherwise FALSE
     */
    PRInt32 CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,PRBool aUpdateStyles);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gess5/11/98
     * @param   aNode is leaf node to be added.
     * @return  TRUE if all went well -- FALSE otherwise.
     */
    PRInt32 AddLeaf(const nsIParserNode& aNode);

    /**
     * Causes auto-closures of context vector stack in order to find a 
     * proper home for the given child. Propagation may also occur as 
     * a fall out.
     * @update	gess5/11/98
     * @param   child to be added (somewhere) to context vector stack.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    PRInt32 ReduceContextStackFor(eHTMLTags aChildTag);

    /**
     * Attempt forward and/or backward propagation for the given
     * child within the current context vector stack.
     * @update	gess5/11/98
     * @param   type of child to be propagated.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    PRInt32 CreateContextStackFor(eHTMLTags aChildTag);

    PRInt32 OpenTransientStyles(eHTMLTags aTag);
    PRInt32 CloseTransientStyles(eHTMLTags aTag);
    PRInt32 UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags aActualTag);
    PRInt32 UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags aActualTag);


    /****************************************************
        These methods interface with the parser to do
        the tokenization phase.
     ****************************************************/


    /**
     * Called to cause delegate to create a token of given type.
     * @update	gess 5/11/98
     * @param   aType represents the kind of token you want to create.
     * @return  new token or NULL
     */
     CToken* CreateTokenOfType(eHTMLTokenTypes aType);

    /**
     * Retrieve the next TAG from the given scanner.
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve next START tag from given scanner.
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve collection of HTML/XML attributes from given scanner
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CStartToken* aToken);
    
    /**
     * Retrieve a sequence of text from given scanner.
     * @update	gess 5/11/98
     * @param   aString will contain retrieved text.
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve an entity from given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve a whitespace sequence from the given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve a comment from the given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);

    /**
     * Retrieve newlines from given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);

    /**
     * Causes content to be skipped up to sequence contained in aString.
     * @update	gess 5/11/98
     * @param   aString ????
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    virtual PRInt32 ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,CToken*& aToken);


protected:

    PRBool  CanContainFormElement(eHTMLTags aParent,eHTMLTags aChild) const;
    
};

extern NS_HTMLPARS nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult);

#endif 
