
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
 * @update  gess 7/15/98
 *
 * NavDTD is an implementation of the nsIDTD interface.
 * In particular, this class captures the behaviors of the original 
 * Navigator parser productions.
 *
 * This DTD, like any other in NGLayout, provides a few basic services:
 *	- First, the DTD collaborates with the Parser class to convert plain 
 *    text into a sequence of HTMLTokens. 
 *	- Second, the DTD describes containment rules for known elements. 
 *	- Third the DTD controls and coordinates the interaction between the
 *	  parsing system and content sink. (The content sink is the interface
 *    that serves as a proxy for content model).
 *	- Fourth the DTD maintains an internal style-stack to handle residual (leaky)
 *	  style tags.
 *
 * You're most likely working in this class file because
 * you want to add or change a behavior inherent in this DTD. The remainder
 * of this section will describe what you need to do to affect the kind of
 * change you want in this DTD.
 *
 * RESIDUAL-STYLE HANDLNG:
 *	 There are a number of ways to represent style in an HTML document.
 *		1) explicit style tags (<B>, <I> etc)
 *		2) implicit styles (like those implicit in <Hn>)
 *		3) CSS based styles
 *
 *	 Residual style handling results from explicit style tags that are
 *	 not closed. Consider this example: <p>text <b>bold </p>
 *	 When the <p> tag closes, the <b> tag is NOT automatically closed.
 *	 Unclosed style tags are handled by the process we call residual-style 
 *	 tag handling. 
 *
 *	 There are two aspects to residual style tag handling. The first is the 
 *	 construction and managing of a stack of residual style tags. The 
 *	 second is the automatic emission of residual style tags onto leaf content
 *	 in subsequent portions of the document.This step is necessary to propagate
 *	 the expected style behavior to subsequent portions of the document.
 *
 *	 Construction and managing the residual style stack is an inline process that
 *	 occurs during the model building phase of the parse process. During the model-
 *	 building phase of the parse process, a content stack is maintained which tracks
 *	 the open container hierarchy. If a style tag(s) fails to be closed when a normal
 *	 container is closed, that style tag is placed onto the residual style stack. If
 *	 that style tag is subsequently closed (in most contexts), it is popped off the
 *	 residual style stack -- and are of no further concern.
 *
 *	 Residual style tag emission occurs when the style stack is not empty, and leaf
 *	 content occurs. In our earlier example, the <b> tag "leaked" out of the <p> 
 *	 container. Just before the next leaf is emitted (in this or another container) the 
 *	 style tags that are on the stack are emitted in succession. These same residual
 *   style tags get closed automatically when the leaf's container closes, or if a
 *   child container is opened.
 * 
 *         
 */
#ifndef NS_NAVHTMLDTD__
#define NS_NAVHTMLDTD__

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsIParser.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"
#include "nsVoidArray.h"
#include "nsDeque.h"


#define NS_INAVHTML_DTD_IID      \
  {0x5c5cce40, 0xcfd6,  0x11d1,  \
  {0xaa, 0xda, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class nsIHTMLContentSink;
class nsIDTDDebug;
class nsIParserNode;
class nsCParserNode;
class CITokenHandler;
class nsParser;
class nsDTDContext;
class nsTagStack;


/***************************************************************
  Now the main event: CNavDTD.

  This not so simple class performs all the duties of token 
  construction and model building. It works in conjunction with
  an nsParser.
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS CNavDTD : public nsIDTD {

#if defined(XP_PC)
#pragma warning( default : 4275 )
#endif

  public:
    /**
      *  Common constructor for navdtd. You probably want to call
  	  *  NS_NewNavHTMLDTD().
	    * 
      *  @update  gess 7/9/98
      */
    CNavDTD();

    /**
     *  Virtual destructor -- you know what to do
     *  @update  gess 7/9/98
     */
    virtual ~CNavDTD();

    /**
     * Call this method if you want the DTD to construct a clone of itself.
     * @update	gess7/23/98
     * @param 
     * @return
     */
    virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);


    NS_DECL_ISUPPORTS

    /**
     * This method is called to determine if the given DTD can parse
     * a document of a given source-type. 
     * Note that parsing assumes that the end result will always be stored 
	   * in the main content model. Of course, it's up to you which content-
	   * model you pass in to the parser, so you can always control the process.
	   *
     * @update	gess 7/15/98
     * @param	aContentType contains the name of a filetype that you are
   	 *			being asked to parse).
     * @return  TRUE if this DTD parse the given type; FALSE otherwise.
     */
    virtual PRBool CanParse(nsString& aContentType, nsString& aCommand, PRInt32 aVersion);

   /**
    * This method gets called to determine if the DTD can determine the
	  * kind of data contained in the given buffer string. If you know the
	  * type, the you should enter its stringname aType.
    * @update	gess7/7/98
    * @param	aBuffer contains data to be examined for autodetection.
    * @param	aType will contain a typename you specify.
    * @return	unknown, valid (if you know the type), invalid (if you dont)
    */
    virtual eAutoDetectResult AutoDetectContentType(nsString& aBuffer,nsString& aType);

    /**
     * Called by the parser to initiate dtd verification of the
     * internal context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual PRBool Verify(nsString& aURLRef);

   
    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	gess5/18/98
      * @param	aFilename is the name of the file being parsed.
      * @return	error code (almost always 0)
      */
    NS_IMETHOD WillBuildModel(nsString& aFilename,PRBool aNotifySink);

   /**
     * The parser uses a code sandwich to wrap the parsing process. Before
     * the process begins, WillBuildModel() is called. Afterwards the parser
     * calls DidBuildModel(). 
     * @update	gess5/18/98
     * @param	anErrorCode contans the last error that occured
     * @return	error code
     */
    NS_IMETHOD DidBuildModel(PRInt32 anErrorCode,PRBool aNotifySink);

    /**
     *  This method is called by the parser, once for each token
	   *	that has been constructed during the tokenization phase.
     *  @update  gess 3/25/98
     *  @param   aToken -- token object to be put into content model
     *  @return  0 if all is well; non-zero is an error
     */
    NS_IMETHOD HandleToken(CToken* aToken);

    /**
     *	Set parser is called to notify the DTD which parser is driving
	   *  the DTD. This is needed by the DTD later, for various parser 
	   *  callback methods.
     *  
     *  @update  gess 3/25/98
     *  @param   aParser pts to the controlling parser
     *  @return  nada.
     */
    virtual void SetParser(nsIParser* aParser);

    /**
     *  Cause the tokenizer to consume the next token, and 
     *  return an error result.
     *  
     *  @update  gess 3/25/98
     *  @param   anError -- ref to error code
     *  @return  new token or null
     */
    NS_IMETHOD ConsumeToken(CToken*& aToken);


    /**
     * If the parse process gets interrupted, this method gets called
	   * prior to the process resuming.
     * @update	gess5/18/98
     * @return	error code -- usually kNoError (0)
     */
    NS_IMETHOD WillResumeParse(void);

    /**
     * If the parse process is about to be interrupted, this method
	   * will be called just prior.
     * @update	gess5/18/98
     * @return	error code  -- usually kNoError (0)
     */
    NS_IMETHOD WillInterruptParse(void);


   /**
     * Select given content sink into parser for parser output
     * @update	gess5/11/98
     * @param   aSink is the new sink to be used by parser
     * @return  old sink, or NULL
     */
    virtual nsIContentSink* SetContentSink(nsIContentSink* aSink);

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
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContainEx(PRInt32 aParent,PRInt32 aChild) const;

    /**
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- int tag of parent container
     *  @param   aChild -- int tag of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanPropagate(eHTMLTags aParent,eHTMLTags aChild) const;

    /**
     *  This method is called to determine whether a tag
     *  of one of its children can contain a given child tag.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */
    virtual PRBool CanContainIndirect(eHTMLTags aParent,eHTMLTags aChild) const;

    /**
     *  This method gets called to determine whether a given 
     *  child tag can be omitted by the given parent.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- parent tag being asked about omitting given child
     *  @param   aChild -- child tag being tested for omittability by parent
     *  @return  PR_TRUE if given tag can be omitted
     */
    virtual PRBool CanOmit(eHTMLTags aParent,eHTMLTags aChild)const;

    /**
     *  This is called to determine if the given parent can omit the
	   *  given child (end tag).
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag type of parent
     *  @param   aChild -- tag type of child
     *  @return  PR_TRUE if given tag can contain omit child (end tag)
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
    virtual PRBool IsContainer(PRInt32 aTag) const;

    /**
     * Call this if you want the DTD to give you a default
	   * Parent tag for given child tag. This is needed in cases
	   * such as propagation.
	   *
     * @update  gess 7/6/98
     * @param   aTag --  child to determine dflt parent tag for
     * @return  enum of parent tag -- potentially eHTMLTag_unknown
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
    virtual PRBool ForwardPropagate(nsTagStack& aTagStack,eHTMLTags aParentTag,eHTMLTags aChildTag);

    /**
     * This method tries to design a context map (without actually
     * changing our parser state) from the child up to the parent.
     *
     * @update  gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  True if closure was achieved -- other false
     */
    virtual PRBool BackwardPropagate(nsTagStack& aTagStack,eHTMLTags aParentTag,eHTMLTags aChildTag) const;

    /**
     * Ask parser if a given container is open ANYWHERE on stack
     * @update	gess5/11/98
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    virtual PRBool HasOpenContainer(eHTMLTags aContainer) const;

    /**
     * This method is used to determine the index on the stack of the
     * nearest container tag that can constrain autoclosure. It is possible
	   * that no tag on the stack will gate autoclosure.
	   *
     * @update	gess 7/15/98
     * @param   id of tag you want to test for
     * @return  index of gating tag on context stack. kNotFound otherwise
     */
    virtual PRBool IsGatedFromClosing(eHTMLTags aChild) const;

    /**
     * Accessor that retrieves the tag type of the topmost item on context 
	   * vector stack.
	   *
     * @update	gess5/11/98
     * @return  tag type (may be unknown)
     */
    virtual eHTMLTags GetTopNode() const;

    /**
     * Finds the topmost occurance of given tag within context vector stack.
     * @update	gess5/11/98
     * @param   tag to be found
     * @return  index of topmost tag occurance -- may be -1 (kNotFound).
     */
    virtual PRInt32 GetTopmostIndexOf(eHTMLTags aTag) const;

    /**
     * Finds the topmost occurance of given tag within context vector stack.
     * @update	gess5/11/98
     * @param   tag to be found
     * @return  index of topmost tag occurance -- may be -1 (kNotFound).
     */
    virtual PRInt32 GetTopmostIndexOf(eHTMLTags aTagSet[],PRInt32 aCount) const;

    /**
     * The following set of methods are used to partially construct 
     * the content model (via the sink) according to the type of token.
     * @update	gess5/11/98
     * @param   aToken is the token (of a given type) to be handled
     * @return  error code representing construction state; usually 0.
     */
    nsresult HandleStartToken(CToken* aToken);
    nsresult HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode);
    nsresult HandleEndToken(CToken* aToken);
    nsresult HandleEntityToken(CToken* aToken);
    nsresult HandleCommentToken(CToken* aToken);
    nsresult HandleSkippedContentToken(CToken* aToken);
    nsresult HandleAttributeToken(CToken* aToken);
    nsresult HandleScriptToken(nsCParserNode& aNode);
    nsresult HandleStyleToken(CToken* aToken);
    nsresult HandleProcessingInstructionToken(CToken* aToken);

    virtual  nsITokenRecycler* GetTokenRecycler(void);


protected:

    /**
     * The following methods are use to create and manage
     * the dynamic set of token handlers.
     * @update	gess5/11/98
     */
    void            InitializeDefaultTokenHandlers();
    CITokenHandler* GetTokenHandler(eHTMLTokenTypes aType) const;
    CITokenHandler* AddTokenHandler(CITokenHandler* aHandler);
    void            DeleteTokenHandlers(void);


    //*************************************************
    //these cover methods mimic the sink, and are used
    //by the parser to manage its context-stack.
    //*************************************************

    /**
     * The next set of method open given HTML elements of
	   * various types.
     * 
     * @update	gess5/11/98
     * @param   node to be opened in content sink.
     * @return  error code representing error condition-- usually 0.
     */
    nsresult OpenHTML(const nsIParserNode& aNode);
    nsresult OpenHead(const nsIParserNode& aNode);
    nsresult OpenBody(const nsIParserNode& aNode);
    nsresult OpenForm(const nsIParserNode& aNode);
    nsresult OpenMap(const nsIParserNode& aNode);
    nsresult OpenFrameset(const nsIParserNode& aNode);
    nsresult OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack);

    /**
     * The next set of methods close the given HTML element.
     * 
     * @update	gess5/11/98
     * @param   HTML (node) to be opened in content sink.
     * @return  error code - 0 if all went well.
     */
    nsresult CloseHTML(const nsIParserNode& aNode);
    nsresult CloseHead(const nsIParserNode& aNode);
    nsresult CloseBody(const nsIParserNode& aNode);
    nsresult CloseForm(const nsIParserNode& aNode);
    nsresult CloseMap(const nsIParserNode& aNode);
    nsresult CloseFrameset(const nsIParserNode& aNode);
    nsresult CloseContainer(const nsIParserNode& aNode,eHTMLTags anActualTag,PRBool aUpdateStyles);
    
    /**
     * The special purpose methods automatically close
     * one or more open containers.
     * @update	gess5/11/98
     * @return  error code - 0 if all went well.
     */
    nsresult CloseTopmostContainer();
    nsresult CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles);
    nsresult CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,PRBool aUpdateStyles);

    /**
     * Causes leaf to be added to sink at current vector pos.
     * @update	gess5/11/98
     * @param   aNode is leaf node to be added.
     * @return  error code - 0 if all went well.
     */
    nsresult AddLeaf(const nsIParserNode& aNode);

    /**
     * Causes auto-closures of context vector stack in order to find a 
     * proper home for the given child. Propagation may also occur as 
     * a fall out.
     * @update	gess5/11/98
     * @param   child to be added (somewhere) to context vector stack.
     * @return  error code - 0 if all went well.
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

    /**
     * This set of methods is used to create and manage the set of
	   * transient styles that occur as a result of poorly formed HTML
   	 * or bugs in the original navigator.
	   *
     * @update	gess5/11/98
     * @param   aTag -- represents the transient style tag to be handled.
     * @return  error code -- usually 0
     */
    nsresult  OpenTransientStyles(eHTMLTags aTag);
    nsresult  CloseTransientStyles(eHTMLTags aTag);
    nsresult  UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags aActualTag);
    nsresult  UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags aActualTag);
    PRBool    CanContainStyles(eHTMLTags aTag) const;
    PRBool    RequiresAutomaticClosure(eHTMLTags aParentTag,eHTMLTags aChildTag) const;

    /****************************************************
        These methods interface with the parser to do
        the tokenization phase.
     ****************************************************/


    /**
     * The following methods consume a particular type
     * of HTML token.
     *
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    nsresult     ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    nsresult     ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    nsresult     ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CStartToken* aToken);
    nsresult     ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    nsresult     ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    nsresult     ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    nsresult     ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    nsresult     ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken);

    /**
     * Causes content to be skipped up to sequence contained in aString.
     * @update	gess 5/11/98
     * @param   aString ????
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    virtual nsresult ConsumeContentToEndTag(const nsString& aString,
																						PRUnichar aChar,
																						eHTMLTags aChildTag,
																						CScanner& aScanner,
																						CToken*& aToken);


protected:

		PRInt32			CollectAttributes(nsCParserNode& aNode,PRInt32 aCount);
		PRInt32			CollectSkippedContent(nsCParserNode& aNode,PRInt32& aCount);
    PRInt32     DidHandleStartTag(CToken* aToken,eHTMLTags aChildTag);    

    nsParser*           mParser;
    nsIHTMLContentSink* mSink;

    CITokenHandler*     mTokenHandlers[eToken_last];

    nsDTDContext*       mHeadContext;
    nsDTDContext*       mBodyContext;
    nsDTDContext*       mFormContext;
    nsDTDContext*       mMapContext;
    PRBool              mAllowUnknownTags;
    PRBool              mHasOpenForm;
    PRBool              mHasOpenMap;
    PRBool              mHasOpenHead;
    nsDeque             mTokenDeque;
    nsString            mFilename;
    nsIDTDDebug*		    mDTDDebug;
    PRInt32             mLineNumber;
    eParseMode          mParseMode;
};

extern NS_HTMLPARS nsresult NS_NewNavHTMLDTD(nsIDTD** aInstancePtrResult);

#endif 



