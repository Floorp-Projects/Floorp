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
 * @update  gpk 7/12/98
 * @update  gess 7/20/98
 * 
 * If you've been paying attention to our many content sink classes, you may be
 * asking yourself, "why do we need yet another one?" The answer is that this 
 * implementation, unlike all the others, really sends its output a given stream
 * rather than to an actual content sink (as defined in our HTML document system).
 *
 * We use this class for a number of purposes: 
 *	 1) For actual document i/o using XIF (xml interchange format)
 *   2) For document conversions
 *   3) For debug purposes (to cause output to go to cout or a file)
 *
 * If no stream is declared in the constructor then all output goes to cout. 
 * The file is pretty printed according to the pretty printing interface. subclasses 
 * may choose to override this behavior or set runtime flags for desired results.
 */

#ifndef  NS_TXTCONTENTSINK_STREAM
#define  NS_TXTCONTENTSINK_STREAM

#include "nsIParserNode.h"
#include "nsIHTMLContentSink.h"
#include "nshtmlpars.h"
#include "nsHTMLTokens.h"


#define NS_HTMLCONTENTSINK_STREAM_IID  \
  {0xa39c6bff, 0x15f0, 0x11d2, \
  {0x80, 0x41, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}

#ifndef XP_MAC
class ostream;
#endif

class nsIUnicodeEncoder;

class nsHTMLContentSinkStream : public nsIHTMLContentSink {
  public:

  /**
   * Standard constructor
   * @update	gess7/7/98
   */
  nsHTMLContentSinkStream(PRBool aDoFormat = PR_TRUE, PRBool aDoHeader = PR_TRUE); 

  /**
   * Constructor with associated stream. If you use this, it means that you want
   * this class to emits its output to the stream you provide.
   * @update	gess7/7/98
   * @param		aStream -- ref to stream where you want output sent
   */
  nsHTMLContentSinkStream(ostream& aStream,PRBool aDoFormat = PR_TRUE, PRBool aDoHeader = PR_TRUE); 

  /**
   * virtual destructor
   * @update	gess7/7/98
   */
  virtual ~nsHTMLContentSinkStream();

  NS_IMETHOD_(void) SetOutputStream(ostream& aStream);

  // nsISupports
  NS_DECL_ISUPPORTS
 
  /*******************************************************************
   * The following methods are inherited from nsIContentSink.
   * Please see that file for details.
   *******************************************************************/
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);

  /*******************************************************************
   * The following methods are inherited from nsIHTMLContentSink.
   * Please see that file for details.
   *******************************************************************/
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);
  NS_IMETHOD DoFragment(PRBool aFlag);
  NS_IMETHOD BeginContext(PRInt32 aPosition);
  NS_IMETHOD EndContext(PRInt32 aPosition);


public:
  void SetLowerCaseTags(PRBool aDoLowerCase) { mLowerCaseTags = aDoLowerCase; }
    

protected:

    nsresult AddLeaf(const nsIParserNode& aNode, ostream& aStream);
    void WriteAttributes(const nsIParserNode& aNode,ostream& aStream);
    void AddStartTag(const nsIParserNode& aNode, ostream& aStream);
    void AddEndTag(const nsIParserNode& aNode, ostream& aStream);
    void AddIndent(ostream& aStream);

    void EnsureBufferSize(PRInt32 aNewSize);
    void UnicodeToHTMLString(const nsString& aSrc);


    nsresult InitEncoder(const nsString& aCharset);



protected:
    ostream*  mOutput;
    int       mTabLevel;

    PRInt32   mIndent;
    PRBool    mLowerCaseTags;
    PRInt32   mHTMLStackPos;
    eHTMLTags mHTMLTagStack[1024];  // warning: hard-coded nesting level
    PRInt32   mColPos;

    PRBool    mDoFormat;
    PRBool    mDoHeader;

    char*     mBuffer;
    PRInt32   mBufferSize;

    nsIUnicodeEncoder*  mUnicodeEncoder;
};

extern NS_HTMLPARS nsresult
NS_New_HTML_ContentSinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                              PRBool aDoFormat = PR_TRUE,
                              PRBool aDoHeader = PR_TRUE);


#endif




