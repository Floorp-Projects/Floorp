/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
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

class nsIHTMLContentSink : public nsIContentSink {
public:
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

};

extern nsresult NS_NewHTMLNullSink(nsIContentSink** aInstancePtrResult);

#endif /* nsIHTMLContentSink_h___ */
