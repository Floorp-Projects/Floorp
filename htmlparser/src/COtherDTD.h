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
     * This method gets called when a start token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleStartToken(CToken* aToken);

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
    nsresult HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode);

    /**
     * This method gets called when an end token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the end token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleEndToken(CToken* aToken);

    /**
     * This method gets called when an entity token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the entity token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleEntityToken(CToken* aToken);

    /**
     * This method gets called when a comment token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the comment token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleCommentToken(CToken* aToken);

    /**
     * This method gets called when a skipped-content token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the skipped-content token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleSkippedContentToken(CToken* aToken);

    /**
     * This method gets called when an attribute token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the attribute token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleAttributeToken(CToken* aToken);

    /**
     * This method gets called when a script token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the script token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleScriptToken(nsCParserNode& aNode);
    
    /**
     * This method gets called when a style token has been consumed and needs 
     * to be handled (possibly added to content model via sink).
     * @update	gess5/11/98
     * @param   aToken is the style token to be handled
     * @return  TRUE if the token was handled.
     */
    nsresult HandleStyleToken(CToken* aToken);

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
    nsresult OpenHTML(const nsIParserNode& aNode);

    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    nsresult CloseHTML(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a head item in 
     * content sink.
     * @update	gess5/11/98
     * @param   HEAD (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenHead(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink head to be closed
     * @update	gess5/11/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseHead(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a body item in 
     * content sink.
     * @update	gess5/11/98
     * @param   BODY (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenBody(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink body to be closed
     * @update	gess5/11/98
     * @param   aNode is the body node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseBody(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a form item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FORM (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenForm(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink form to be closed
     * @update	gess5/11/98
     * @param   aNode is the form node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseForm(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a form item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FORM (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenMap(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink form to be closed
     * @update	gess5/11/98
     * @param   aNode is the form node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseMap(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a frameset item in 
     * content sink.
     * @update	gess5/11/98
     * @param   FRAMESET (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenFrameset(const nsIParserNode& aNode);

    /**
     * This cover method causes the content-sink frameset to be closed
     * @update	gess5/11/98
     * @param   aNode is the frameeset node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseFrameset(const nsIParserNode& aNode);

    /**
     * This cover method opens the given node as a generic container in 
     * content sink.
     * @update	gess5/11/98
     * @param   generic container (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    nsresult OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack);

    /**
     * This cover method causes a generic containre in the content-sink to be closed
     * @update	gess5/11/98
     * @param   aNode is the node to be closed in sink (usually ignored)
     * @return  TRUE if all went well.
     */
    nsresult CloseContainer(const nsIParserNode& aNode,eHTMLTags anActualTag,PRBool aUpdateStyles);
    
    /**
     * This cover method causes the topmost container to be closed in sink
     * @update	gess5/11/98
     * @return  TRUE if all went well.
     */
    nsresult CloseTopmostContainer();
    
    /**
     * Cause all containers down to topmost given tag to be closed
     * @update	gess5/11/98
     * @param   aTag is the tag at which auto-closure should stop (inclusive) 
     * @return  TRUE if all went well -- otherwise FALSE
     */
    nsresult CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles);

    /**
     * Cause all containers down to given position to be closed
     * @update	gess5/11/98
     * @param   anIndex is the stack pos at which auto-closure should stop (inclusive) 
     * @return  TRUE if all went well -- otherwise FALSE
     */
    nsresult CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,PRBool aUpdateStyles);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gess5/11/98
     * @param   aNode is leaf node to be added.
     * @return  TRUE if all went well -- FALSE otherwise.
     */
    nsresult AddLeaf(const nsIParserNode& aNode);

    /**
     * Causes auto-closures of context vector stack in order to find a 
     * proper home for the given child. Propagation may also occur as 
     * a fall out.
     * @update	gess5/11/98
     * @param   child to be added (somewhere) to context vector stack.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    nsresult ReduceContextStackFor(eHTMLTags aChildTag);

    /**
     * Attempt forward and/or backward propagation for the given
     * child within the current context vector stack.
     * @update	gess5/11/98
     * @param   type of child to be propagated.
     * @return  TRUE if succeeds, otherwise FALSE
     */
    nsresult CreateContextStackFor(eHTMLTags aChildTag);

    nsresult OpenTransientStyles(eHTMLTags aTag);
    nsresult CloseTransientStyles(eHTMLTags aTag);
    nsresult UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags aActualTag);
    nsresult UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags aActualTag);

    nsresult DoFragment(PRBool aFlag);

protected:

    
};

extern NS_HTMLPARS nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult);

#endif 
