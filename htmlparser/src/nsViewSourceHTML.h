/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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


class nsIParserNode;
class nsParser;
class nsITokenizer;
class nsCParserNode;

class CViewSourceHTML: public nsIDTD
{
public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDTD
    
    CViewSourceHTML();
    virtual ~CViewSourceHTML();

    /**
     * Set this to TRUE if you want the DTD to verify its
     * context stack.
     * @update	gess 7/23/98
     * @param 
     * @return
     */
    virtual void SetVerification(PRBool aEnable);

private:
    nsresult WriteTag(PRInt32 tagType,
                      const nsAString &aText,
                      PRInt32 attrCount,
                      PRBool aNewlineRequired);
    
    nsresult WriteAttributes(PRInt32 attrCount);
    nsresult GenerateSummary();
    void StartNewPreBlock(void);
    // Utility method for adding attributes to the nodes we generate
    void AddAttrToNode(nsCParserStartNode& aNode,
                       nsTokenAllocator* aAllocator,
                       const nsAString& aAttrName,
                       const nsAString& aAttrValue);

protected:

    nsParser*           mParser;
    nsIHTMLContentSink* mSink;
    PRInt32             mLineNumber;
    nsITokenizer*       mTokenizer; // weak

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
    PRPackedBool        mSyntaxHighlight;
    PRPackedBool        mWrapLongLines;
    PRPackedBool        mHasOpenRoot;
    PRPackedBool        mHasOpenBody;

    nsDTDMode           mDTDMode;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...
    eParserDocType      mDocType;
    nsCString           mMimeType;  
    PRInt32             mErrorCount;
    PRInt32             mTagCount;

    nsString            mFilename;
    nsString            mTags;
    nsString            mErrors;

    PRUint32            mTokenCount;
};

extern nsresult NS_NewViewSourceHTML(nsIDTD** aInstancePtrResult);

#endif 



