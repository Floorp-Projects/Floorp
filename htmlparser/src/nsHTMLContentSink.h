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
 * This file declares the concrete HTMLContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 *
 * After the tokenizer completes, the parser iterates over
 * the known token list. As the parser identifies valid 
 * elements, it calls the contentsink interface to notify
 * the content model that a new node or child node is being
 * created and added to the content model.
 *
 * The HTMLContentSink interface assumes 4 underlying
 * containers: HTML, HEAD, BODY and FRAMESET. Before  
 * accessing any these, the parser will call the appropriate
 * OpenXXX method: OpenHTML,OpenHead,OpenBody,OpenFrameSet;
 * likewise, the CloseXXX version will be called when the
 * parser is done with a given section.
 *
 * IMPORTANT: The parser may Open each container more than
 * once! This is due to the irregular nature of HTML files.
 * For example, it is possible to encounter plain text at
 * the start of an HTML document (that preceeds the HTML tag).
 * Such text is treated as if it were part of the body.
 * In such cases, the parser will Open the body, pass the text-
 * node in and then Close the body. The body will likely be
 * re-Opened later when the actual <BODY> tag has been seen.
 *
 * Containers within the body are Opened and Closed
 * using the OpenContainer(...) and CloseContainer(...) calls.
 * It is assumed that the document or contentSink is 
 * maintaining its state to manage where new content should 
 * be added to the underlying document.
 *
 * NOTE: OpenHTML() and OpenBody() may get called multiple times
 *       in the same document. That's fine, and it doesn't mean
 *       that we have multiple bodies or HTML's.
 *
 * NOTE: I haven't figured out how sub-documents (non-frames)
 *       are going to be handled. Stay tuned.
 */

#ifndef	NS_HTMLCONTENTSINK
#define	NS_HTMLCONTENTSINK

#include "nsIParserNode.h"
#include "nsIHTMLContentSink.h"
#include "nsIContentSink.h"
#include "nsString.h"

#define NS_HTMLCONTENTSINK_IID   \
  {0x803f8110, 0xc287,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x08, 0x14, 0x98, 0x89}}


class nsHTMLContentSink : public nsIHTMLContentSink {
	public:

  enum eSection {eNone=0,eHTML,eHead,eBody,eContainer};

                      nsHTMLContentSink();
                      ~nsHTMLContentSink();

  NS_DECL_ISUPPORTS
 
            PRBool    SetTitle(const nsString& aValue);

                      // Called when Opening or closing the main HTML container
            PRBool    OpenHTML(const nsIParserNode& aNode);
            PRBool    CloseHTML(const nsIParserNode& aNode);

                      // Called when Opening or closing the main HEAD container
            PRBool    OpenHead(const nsIParserNode& aNode);
            PRBool    CloseHead(const nsIParserNode& aNode);
  
                      // Called when Opening or closing the main BODY container
            PRBool    OpenBody(const nsIParserNode& aNode);
            PRBool    CloseBody(const nsIParserNode& aNode);

                      // Called when Opening or closing FORM containers
            PRBool    OpenForm(const nsIParserNode& aNode);
            PRBool    CloseForm(const nsIParserNode& aNode);
        
                      // Called when Opening or closing the main FRAMESET container
            PRBool    OpenFrameset(const nsIParserNode& aNode);
            PRBool    CloseFrameset(const nsIParserNode& aNode);
        
                      // Called when Opening or closing a general container
                      // This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
                      // Until proven otherwise, I also plan to toss STYLE, 
                      // FRAME, SCRIPT, etc. here too!
    virtual PRBool    OpenContainer(const nsIParserNode& aNode);
    virtual PRBool    CloseContainer(const nsIParserNode& aNode);
    virtual PRBool    CloseTopmostContainer();
    virtual PRBool    AddLeaf(const nsIParserNode& aNode);

            PRInt32   NodeAt(PRInt32 aPos) {return mNodeStack[aPos];}
            PRInt32   TopNode() {return mNodeStack[mNodeStackPos-1];}
            PRInt32   GetStackPos() {return mNodeStackPos;}

  protected:
            PRInt32     mNodeStack[100];
            PRInt32     mNodeStackPos;
            nsString    mTitle;
};


#endif


