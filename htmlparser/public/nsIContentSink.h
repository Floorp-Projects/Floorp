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
#ifndef nsIContentSink_h___
#define nsIContentSink_h___

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 * This pure virtual interface is used as the "glue" that connects the parsing 
 * process to the content model construction process.
 *
 * The icontentsink interface is a very lightweight wrapper that represents the
 * content-sink model building process. There is another one that you may care 
 * about more, which is the IHTMLContentSink interface. (See that file for details).
 */

#include "nsIParserNode.h"
#include "nsISupports.h"
#include "nsParserError.h"

class nsIParser;

#define NS_ICONTENT_SINK_IID \
{ 0xa6cf9052, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// The base value for the content ID counter.
// Values greater than or equal to this base value are used
// by each of the content sinks to assign unique values
// to the content objects created by them.
#define NS_CONTENT_ID_COUNTER_BASE 10000

class nsIContentSink : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENT_SINK_IID)

  /**
   * This method gets called when the parser begins the process
   * of building the content model via the content sink.
   *
   * @update 5/7/98 gess
   */     
  NS_IMETHOD WillBuildModel(void)=0;

  /**
   * This method gets called when the parser concludes the process
   * of building the content model via the content sink.
   *
   * @param  aQualityLevel describes how well formed the doc was.
   *         0=GOOD; 1=FAIR; 2=POOR;
   * @update 5/7/98 gess
   */     
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel)=0;

  /**
   * This method gets called when the parser gets i/o blocked,
   * and wants to notify the sink that it may be a while before
   * more data is available.
   *
   * @update 5/7/98 gess
   */     
  NS_IMETHOD WillInterrupt(void)=0;

  /**
   * This method gets called when the parser i/o gets unblocked,
   * and we're about to start dumping content again to the sink.
   *
   * @update 5/7/98 gess
   */     
  NS_IMETHOD WillResume(void)=0;

  /**
   * This method gets called by the parser so that the content
   * sink can retain a reference to the parser. The expectation
   * is that the content sink will drop the reference when it
   * gets the DidBuildModel notification i.e. when parsing is done.
   */
  NS_IMETHOD SetParser(nsIParser* aParser)=0;

  /**
   * This method is used to open a generic container in the sink.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode) = 0;

  /**
   *  This method gets called by the parser when a close
   *  container tag has been consumed and needs to be closed.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode) = 0;

  /**
   * This gets called by the parser when you want to add
   * a leaf node to the current container in the content
   * model.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode) = 0;

  /**
   * This gets called by the parser when you want to add
   * a leaf node to the current container in the content
   * model.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD AddComment(const nsIParserNode& aNode) = 0;

  /**
   * This gets called by the parser when you want to add
   * a leaf node to the current container in the content
   * model.
   *
   * @update 4/1/98 gess
   * @param  nsIParserNode reference to parser node interface
   */     
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode) = 0;

  /**
   * This method is called by the parser when it encounters
   * a document type declaration.
   *
   * XXX Should the parser also part the internal subset?
   *
   * @param  nsIParserNode reference to parser node interface
   */
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)=0;

  /**
   * This gets called by the parser if it hits an unrecoverable
   * error (in XML, if the document is not well-formed or valid).
   *
   * @param aErrorResult the error code
   */
  NS_IMETHOD NotifyError(const nsParserError* aError)=0;

  /**
   * This gets called by the parser to notify observers of
   * the tag
   *
   * @param aErrorResult the error code
   */
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode)=0;

  /**
   * Flush all pending notifications so that the content model
   * is in sync with the state of the sink.
   */
  NS_IMETHOD FlushPendingNotifications()=0;

  /**
   * Set the document character set. This should be passed on to the
   * document itself.
   */
  NS_IMETHOD SetDocumentCharset(nsAWritableString& aCharset)=0;
};

#endif /* nsIContentSink_h___ */
