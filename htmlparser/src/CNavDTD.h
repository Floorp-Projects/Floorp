
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

#ifndef NS_NAVHTMLDTD__
#define NS_NAVHTMLDTD__

#include "nsIDTD.h"
#include "nsISupports.h"
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
class CITokenHandler;
class nsParser;



/***************************************************************
  First define a helper class called CTagStack.

  Simple, we've built ourselves a little data structure that
  serves as a stack for htmltags (and associated bits). 
  What's special is that if you #define _dynstack 1, the stack
  size can grow dynamically (like you'ld want in a release build.)
  If you don't #define _dynstack 1, then the stack is a fixed size,
  equal to the eStackSize enum. This makes debugging easier, because
  you can see the htmltags on the stack if its not dynamic.
 ***************************************************************/

//#define _dynstack 1
CLASS_EXPORT_HTMLPARS CTagStack {
  enum {eStackSize=200};

public:

            CTagStack(int aDefaultSize=50);
            ~CTagStack();
  void      Push(eHTMLTags aTag);
  eHTMLTags Pop();
  eHTMLTags First() const;
  eHTMLTags Last() const;

  int       mSize;
  int       mCount;

#ifdef _dynstack
  eHTMLTags*  mTags;
  PRBool*     mBits;
#else
  eHTMLTags   mTags[200];
  PRBool      mBits[200];
#endif
};

/***************************************************************
  Now the main event: CNavDTD.

  This not so simple class performs all the duties of token 
  construction and model building. It works in conjunction with
  an nsParser.
 ***************************************************************/


CLASS_EXPORT_HTMLPARS CNavDTD : public nsIDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    CNavDTD();

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~CNavDTD();

    /**
     * This method is called to determine if the given DTD can parse
     * a document in a given source-type. 
     * NOTE: Parsing always assumes that the end result will involve
     *       storing the result in the main content model.
     * @update	gess6/24/98
     * @param   
     * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
     */
    virtual PRBool CanParse(nsString& aContentType, PRInt32 aVersion);

    /**
     * 
     * @update	gess7/7/98
     * @param 
     * @return
     */
    virtual eAutoDetectResult AutoDetectContentType(nsString& aBuffer,nsString& aType);

    /**
     * 
     * @update	jevering6/23/98
     * @param 
     * @return
     */
	  virtual void SetDTDDebug(nsIDTDDebug * aDTDDebug);

    /**
     * 
     * @update	gess5/18/98
     * @param 
     * @return
     */
    virtual PRInt32 WillBuildModel(nsString& aFilename);

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
     * 
     *  
     *  @update  gess 3/25/98
     *  @param   
     *  @return 
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
     * Ask parser if a given container is open ANYWHERE on stack
     * @update	gess5/11/98
     * @param   id of container you want to test for
     * @return  TRUE if the given container type is open -- otherwise FALSE
     */
    virtual PRBool HasOpenContainer(eHTMLTags aContainer) const;


    /**
     * Retrieve the tag type of the topmost item on context vector stack
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
     * The following set of methods are used to partially construct 
     * the content model (via the sink) according to the type of token.
     * @update	gess5/11/98
     * @param   aToken is the start token to be handled
     * @return  TRUE if the token was handled.
     */
    PRInt32 HandleStartToken(CToken* aToken);
    PRInt32 HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode);
    PRInt32 HandleEndToken(CToken* aToken);
    PRInt32 HandleEntityToken(CToken* aToken);
    PRInt32 HandleCommentToken(CToken* aToken);
    PRInt32 HandleSkippedContentToken(CToken* aToken);
    PRInt32 HandleAttributeToken(CToken* aToken);
    PRInt32 HandleScriptToken(CToken* aToken);
    PRInt32 HandleStyleToken(CToken* aToken);


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
     * The next set of method open given HTML element.
     * 
     * @update	gess5/11/98
     * @param   HTML (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 OpenHTML(const nsIParserNode& aNode);
    PRInt32 OpenHead(const nsIParserNode& aNode);
    PRInt32 OpenBody(const nsIParserNode& aNode);
    PRInt32 OpenForm(const nsIParserNode& aNode);
    PRInt32 OpenMap(const nsIParserNode& aNode);
    PRInt32 OpenFrameset(const nsIParserNode& aNode);
    PRInt32 OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack);

    /**
     * The next set of methods close the given HTML element.
     * 
     * @update	gess5/11/98
     * @param   HTML (node) to be opened in content sink.
     * @return  TRUE if all went well.
     */
    PRInt32 CloseHTML(const nsIParserNode& aNode);
    PRInt32 CloseHead(const nsIParserNode& aNode);
    PRInt32 CloseBody(const nsIParserNode& aNode);
    PRInt32 CloseForm(const nsIParserNode& aNode);
    PRInt32 CloseMap(const nsIParserNode& aNode);
    PRInt32 CloseFrameset(const nsIParserNode& aNode);
    PRInt32 CloseContainer(const nsIParserNode& aNode,eHTMLTags anActualTag,PRBool aUpdateStyles);
    
    /**
     * The special purpose methods automatically close
     * one or more open containers.
     * @update	gess5/11/98
     * @return  TRUE if all went well.
     */
    PRInt32 CloseTopmostContainer();
    PRInt32 CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles);
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
    PRBool  CanContainStyles(eHTMLTags aTag) const;

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
     * The following methods consume a particular type
     * of HTML token.
     *
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    PRInt32     ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    PRInt32     ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CStartToken* aToken);
    PRInt32     ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    PRInt32     ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    PRInt32     ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    PRInt32     ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    PRInt32     ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken);

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
    
    nsParser*           mParser;
    nsIHTMLContentSink* mSink;

    CITokenHandler*     mTokenHandlers[eToken_last];

    CTagStack           mContextStack;
    CTagStack           mStyleStack;

    PRBool              mHasOpenForm;
    PRBool              mHasOpenMap;
    nsDeque             mTokenDeque;
    nsString            mFilename;
    nsIDTDDebug*		    mDTDDebug;
};

extern NS_HTMLPARS nsresult NS_NewNavHTMLDTD(nsIDTD** aInstancePtrResult);

#endif 



