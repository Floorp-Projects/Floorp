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
#ifdef XSL
#include "nsIObserver.h"
#include "nsITransformMediator.h"
#endif

class nsIDocument;
class nsIScriptObjectOwner;
class nsIURI;
class nsIWebShell;
class nsIContent;
class nsVoidArray;
class nsIXMLDocument;
class nsIUnicharInputStream;
class nsIParser;
class nsINameSpace;
class nsICSSLoader;

typedef enum {
  eXMLContentSinkState_InProlog,
  eXMLContentSinkState_InDocumentElement,
  eXMLContentSinkState_InEpilog
} XMLContentSinkState;

// XXX Till the parser knows a little bit more about XML, 
// this is a HTMLContentSink.
class nsXMLContentSink : public nsIXMLContentSink
#ifdef XSL
                         ,
                         public nsIObserver
#endif
{
public:
  nsXMLContentSink();
  virtual ~nsXMLContentSink();

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURL,
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
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);

  // nsIXMLContentSink
  NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);  
  NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
  NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
  NS_IMETHOD AddNotation(const nsIParserNode& aNode);
  NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

#ifdef XSL
  // nsIObserver
  NS_IMETHOD Observe(nsISupports *aSubject, 
                     const PRUnichar *aTopic, 
                     const PRUnichar *someData);
#endif

  NS_IMETHOD ResumeParsing();
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

  nsresult RefreshIfEnabled(nsIViewManager* vm);
  
  nsresult ProcessCSSStyleLink(nsIContent* aElement,
                            const nsString& aHref, PRBool aAlternate,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);
#ifdef XSL
  nsresult ProcessStyleLink(nsIContent* aElement,
                            const nsString& aHref, PRBool aAlternate,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);

  nsresult ProcessXSLStyleLink(nsIContent* aElement,
                            const nsString& aHref, PRBool aAlternate,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);
  nsresult CreateStyleSheetURL(nsIURI** aUrl, const nsAutoString& aHref);
  nsresult LoadXSLStyleSheet(nsIURI* aUrl, const nsString& aType);
  nsresult SetupTransformMediator();
#endif
  void StartLayoutProcess();

  nsresult AddText(const nsString& aString);
  nsresult CreateErrorText(const nsParserError* aError, nsString& aErrorString);
  nsresult CreateSourceText(const nsParserError* aError, nsString& aSourceString);

  nsIDocument* mDocument;
  nsIURI* mDocumentURL;
  nsIURI* mDocumentBaseURL; // can be set via HTTP headers
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

  nsString  mPreferredStyle;
  PRInt32 mStyleSheetCount;
  nsICSSLoader* mCSSLoader;
#ifdef XSL
  nsITransformMediator* mXSLTransformMediator;    // Weak reference
#endif
};

#endif // nsXMLContentSink_h__
