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

#ifndef nsXMLContentSink_h__
#define nsXMLContentSink_h__

#include "nsIXMLContentSink.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"

class nsIDocument;
class nsIScriptObjectOwner;
class nsIURL;
class nsIWebShell;
class nsIContent;
class nsVoidArray;
class nsIXMLDocument;
class nsIUnicharInputStream;
class nsIParser;
class nsINameSpace;

typedef enum {
  eXMLContentSinkState_InProlog,
  eXMLContentSinkState_InDocumentElement,
  eXMLContentSinkState_InEpilog
} XMLContentSinkState;

#ifdef XSL
typedef struct _XSLStyleSheetState {
  PRBool sheetExists;
  nsIXSLContentSink* sink;
} XSLStyleSheetState;
#endif

// XXX Till the parser knows a little bit more about XML, 
// this is a HTMLContentSink.
class nsXMLContentSink : public nsIXMLContentSink {
public:
  nsXMLContentSink();
  virtual ~nsXMLContentSink();

  nsresult Init(nsIDocument* aDoc,
                nsIURL* aURL,
                nsIWebShell* aContainer);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);  
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddCDATASection(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);

  // nsIXMLContentSink
  NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
  NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
  NS_IMETHOD AddNotation(const nsIParserNode& aNode);
  NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

  NS_IMETHOD ResumeParsing();
  NS_IMETHOD LoadStyleSheet(nsIURL* aURL,
                            nsIUnicharInputStream* aUIN,
                            PRBool aActive,
                            const nsString& aTitle,
                            const nsString& aMedia,
                            nsIContent* aOwner);
  NS_IMETHOD EvaluateScript(nsString& aScript, PRUint32 aLineNo);

protected:
  void StartLayout();

  nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                     PRBool* aDidFlush=nsnull);

  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIContent* aContent,
                         PRBool aIsHTML);
  nsresult AddContentAsLeaf(nsIContent *aContent);
  void    PushNameSpacesFrom(const nsIParserNode& aNode);
  nsIAtom*  CutNameSpacePrefix(nsString& aString);
  PRInt32 GetNameSpaceId(nsIAtom* aPrefix);
  nsINameSpace*    PopNameSpaces();
  PRBool  IsHTMLNameSpace(PRInt32 aId);

  nsIContent* GetCurrentContent();
  PRInt32 PushContent(nsIContent *aContent);
  nsIContent* PopContent();

  nsresult ProcessEndSCRIPTTag(const nsIParserNode& aNode);
  nsresult ProcessStartSCRIPTTag(const nsIParserNode& aNode);

#ifdef XSL
  nsresult CreateStyleSheetURL(nsIURL** aUrl, const nsAutoString& aHref);
  nsresult LoadXSLStyleSheet(const nsIURL* aUrl);
#endif

  nsresult AddText(const nsString& aString);
  nsresult CreateErrorText(const nsParserError* aError, nsString& aErrorString);
  nsresult CreateSourceText(const nsParserError* aError, nsString& aSourceString);

  nsIDocument* mDocument;
  nsIURL* mDocumentURL;
  nsIWebShell* mWebShell;
  nsIParser* mParser;

  nsIContent* mRootElement;
  nsIContent* mDocElement;
  XMLContentSinkState mState;

  nsVoidArray* mContentStack;
  nsVoidArray* mNameSpaceStack;

  nsScrollPreference mOriginalScrollPreference;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;
  PRBool mConstrainSize;

  // XXX Special processing for HTML SCRIPT tags. We may need
  // something similar for STYLE.
  PRBool mInScript;
  PRUint32 mScriptLineNo;

#ifdef XSL
  XSLStyleSheetState mXSLState;
#endif
};

#endif // nsXMLContentSink_h__
