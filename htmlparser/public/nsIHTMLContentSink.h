/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIHTMLContentSink_h___
#define nsIHTMLContentSink_h___

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
 * OpennsIHTMLContentSink method: OpenHTML,OpenHead,OpenBody,OpenFrameSet;
 * likewise, the ClosensIHTMLContentSink version will be called when the
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
#include "nsIParserNode.h"
#include "nsIContentSink.h"

#define NS_IHTML_CONTENT_SINK_IID \
 { 0xa6cf9051, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}


#ifdef XP_MAC
#define MAX_REFLOW_DEPTH  75    //setting to 75 to prevent layout from crashing on mac. Bug 55095.
#else
#define MAX_REFLOW_DEPTH  200   //windows and linux (etc) can do much deeper structures.
#endif


class nsIHTMLContentSink : public nsIContentSink {
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IHTML_CONTENT_SINK_IID; return iid; }

  /**
   * This method gets called by the parser when it encounters
   * a title tag and wants to set the document title in the sink.
   *
   * @update 4/1/98 gess
   * @param  nsString reference to new title value
   */     
  NS_IMETHOD SetTitle(const nsString& aValue)=0;

  /**
   * This method is used to open the outer HTML container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode)=0;

  /**
   * This method is used to close the outer HTML container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode)=0;

  /**
   * This method is used to open the only HEAD container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenHead(const nsIParserNode& aNode)=0;

  /**
   * This method is used to close the only HEAD container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseHead(const nsIParserNode& aNode)=0;
  
  /**
   * This method is used to open the main BODY container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenBody(const nsIParserNode& aNode)=0;

  /**
   * This method is used to close the main BODY container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseBody(const nsIParserNode& aNode)=0;

  /**
   * This method is used to open a new FORM container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenForm(const nsIParserNode& aNode)=0;

  /**
   * This method is used to close the outer FORM container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseForm(const nsIParserNode& aNode)=0;

  /**
   * This method is used to open a new MAP container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenMap(const nsIParserNode& aNode)=0;

  /**
   * This method is used to close the MAP container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseMap(const nsIParserNode& aNode)=0;
        
  /**
   * This method is used to open the FRAMESET container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode)=0;

  /**
   * This method is used to close the FRAMESET container.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode)=0;

  /**
   * This method tells the sink whether or not it is 
   * encoding an HTML fragment or the whole document.
   * By default, the entire document is encoded.
   *
   * @update 03/14/99 gpk
   * @param  aFlag set to true if only encoding a fragment
   */     

  NS_IMETHOD DoFragment(PRBool aFlag)=0;

  /**
   * This gets called when handling illegal contents, especially
   * in dealing with tables. This method creates a new context.
   * 
   * @update 04/04/99 harishd
   * @param aPosition - The position from where the new context begins.
   */
  NS_IMETHOD BeginContext(PRInt32 aPosition)=0;
  
  /**
   * This method terminates any new context that got created by
   * BeginContext and switches back to the main context.  
   *
   * @update 04/04/99 harishd
   * @param aPosition - Validates the end of a context.
   */
  NS_IMETHOD EndContext(PRInt32 aPosition)=0;
  
  /**
   * Use this method to retrieve pref. for the tag. 
   *
   * @update 04/11/01 harishd
   * @param aTag - Check pref. for this tag.
   */
  NS_IMETHOD GetPref(PRInt32 aTag,PRBool& aPref)=0;

   /**
   * This method is called when parser is about to begin
   * synchronously processing a chunk of tokens. 
   */
  NS_IMETHOD WillProcessTokens(void)=0;

  /**
   * This method is called when parser has
   * completed processing a chunk of tokens. The processing of the
   * tokens may be interrupted by returning NS_ERROR_HTMLPARSER_INTERRUPTED from
   * DidProcessAToken.
   */
  NS_IMETHOD DidProcessTokens()=0;

  /**
   * This method is called when parser is about to
   * process a single token
   */
  NS_IMETHOD WillProcessAToken(void)=0;

  /**
   * This method is called when parser has completed
   * the processing for a single token.
   * @return NS_OK if processing should not be interrupted
   *         NS_ERROR_HTMLPARSER_INTERRUPTED if the parsing should be interrupted
   */
  NS_IMETHOD DidProcessAToken(void)=0;

};

#ifdef NS_DEBUG

extern nsresult NS_NewHTMLNullSink(nsIContentSink** aInstancePtrResult);

#endif

#endif /* nsIHTMLContentSink_h___ */
