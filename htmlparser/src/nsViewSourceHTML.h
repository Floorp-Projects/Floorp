
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

#ifndef __NS_VIEWSOURCE_HTML_
#define __NS_VIEWSOURCE_HTML_

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "nsIHTMLContentSink.h"
#include "nsDTDUtils.h"
#include "nsParserNode.h"

#define NS_VIEWSOURCE_HTML_IID      \
  {0xb6003010, 0x7932, 0x11d2, \
  {0x80, 0x1b, 0x0, 0x60, 0x8, 0xbf, 0xc4, 0x89 }}


class nsIDTDDebug;
class nsIParserNode;
class nsParser;
class nsITokenizer;
class nsCParserNode;

class CViewSourceHTML: public nsIDTD {
            
  public:

    NS_DECL_ISUPPORTS


    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    CViewSourceHTML();

    virtual const nsIID&  GetMostDerivedIID(void) const;

    /**
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */
    virtual ~CViewSourceHTML();


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
    virtual eAutoDetectResult CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion);

    /**
      * The parser uses a code sandwich to wrap the parsing process. Before
      * the process begins, WillBuildModel() is called. Afterwards the parser
      * calls DidBuildModel(). 
      * @update	rickg 03.20.2000
      * @param	aParserContext
      * @param	aSink
      * @return	error code (almost always 0)
      */
    NS_IMETHOD WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink);

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
     * 
     * @update	gess 12/20/99
     * @param   ptr-ref to (out) tokenizer
     * @return  nsresult
     */
    NS_IMETHOD  GetTokenizer(nsITokenizer*& aTokenizer);

    
    /**
     * 
     * @update	gess12/28/98
     * @param 
     * @return
     */
    virtual  nsTokenAllocator* GetTokenAllocator(void);

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
     * Use this id you want to stop the building content model
     * --------------[ Sets DTD to STOP mode ]----------------
     * It's recommended to use this method in accordance with
     * the parser's terminate() method.
     *
     * @update	harishd 07/22/99
     * @param 
     * @return
     */
    virtual nsresult  Terminate(nsIParser* aParser=nsnull);

    /**
     * Give rest of world access to our tag enums, so that CanContain(), etc,
     * become useful.
     */
    NS_IMETHOD StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const;

    NS_IMETHOD IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const;

    NS_IMETHOD ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const;

    virtual PRBool  IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const;
    virtual PRBool  IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const;

    /**
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */
    virtual PRBool IsContainer(PRInt32 aTag) const;


private:
    nsresult WriteTag(PRInt32 tagType,const nsAReadableString &aText,PRInt32 attrCount,PRBool aNewlineRequired);
    nsresult WriteTagWithError(PRInt32 tagType,const nsAReadableString& aToken,PRInt32 attrCount,PRBool aNewlineRequired);
    void     AddContainmentError(eHTMLTags aChild,eHTMLTags aParent,PRInt32 aLineNumber);

    nsresult WriteAttributes(PRInt32 attrCount);
    nsresult GenerateSummary();

protected:

    nsParser*           mParser;
    nsIHTMLContentSink* mSink;
    PRInt32             mLineNumber;
    nsITokenizer*       mTokenizer;

    PRInt32             mStartTag;
    PRInt32             mEndTag;
    PRInt32             mCommentTag;
    PRInt32             mCDATATag;
    PRInt32             mMarkupDeclaration;
    PRInt32             mDocTypeTag;
    PRInt32             mPITag;
    PRInt32             mEntityTag;
    PRInt32             mText;
    PRInt32             mKey;
    PRInt32             mValue;
    PRInt32             mPopupTag;
    PRInt32             mSummaryTag;
    PRBool              mSyntaxHighlight;
    PRBool              mWrapLongLines;

    nsDTDMode           mDTDMode;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...
    eParserDocType      mDocType;
    nsAutoString        mMimeType;  
    PRInt32             mErrorCount;
    PRInt32             mTagCount;

    nsIDTD              *mValidator;
    nsString            mFilename;
    nsString            mTags;
    nsString            mErrors;
    PRBool              mShowErrors;
    PRBool              mHasOpenRoot;
    PRBool              mHasOpenBody;
    PRBool              mInCDATAContainer;
};

extern nsresult NS_NewViewSourceHTML(nsIDTD** aInstancePtrResult);

#endif 



