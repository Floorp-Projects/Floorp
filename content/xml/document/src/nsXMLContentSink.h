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

#ifndef nsXMLContentSink_h__
#define nsXMLContentSink_h__

#include "nsIXMLContentSink.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsITransformMediator.h"
#include "nsIUnicharInputStream.h"
#include "nsIStreamLoader.h"
#include "nsISupportsArray.h"
#include "nsINodeInfo.h"
#include "nsIDOMHTMLTextAreaElement.h"
class nsICSSStyleSheet;
#include "nsICSSLoaderObserver.h"
#include "nsIHTMLContent.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsSupportsArray.h"

class nsIDocument;
class nsIURI;
class nsIWebShell;
class nsIContent;
class nsAutoVoidArray;
class nsIXMLDocument;
class nsIUnicharInputStream;
class nsIParser;
class nsINameSpace;
class nsICSSLoader;
class nsINameSpaceManager;
class nsIElementFactory;

typedef enum {
  eXMLContentSinkState_InProlog,
  eXMLContentSinkState_InDocumentElement,
  eXMLContentSinkState_InEpilog
} XMLContentSinkState;

// XXX Till the parser knows a little bit more about XML, 
// this is a HTMLContentSink.
class nsXMLContentSink : public nsIXMLContentSink,
                         public nsIObserver,
                         public nsSupportsWeakReference,
                         public nsIScriptLoaderObserver,
                         public nsICSSLoaderObserver
{
public:
  nsXMLContentSink();
  virtual ~nsXMLContentSink();

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURL,
                nsIWebShell* aContainer);

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER

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
  NS_IMETHOD FlushPendingNotifications() { return NS_OK; }
  NS_IMETHOD SetDocumentCharset(nsAWritableString& aCharset);

  // nsIXMLContentSink
  NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);  
  NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
  NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
  NS_IMETHOD AddNotation(const nsIParserNode& aNode);
  NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet*aSheet, PRBool aNotify);

  // nsIObserver
  NS_IMETHOD Observe(nsISupports *aSubject, 
                     const PRUnichar *aTopic, 
                     const PRUnichar *someData);

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
  nsresult ProcessSTYLETag(const nsIParserNode& aNode);

  nsresult ProcessBASETag();
  nsresult ProcessMETATag();
  nsresult ProcessLINKTag();
  nsresult ProcessHeaderData(nsIAtom* aHeader,const nsAReadableString& aValue,nsIHTMLContent* aContent);

  nsresult RefreshIfEnabled(nsIViewManager* vm);
  
  nsresult ProcessCSSStyleLink(nsIContent* aElement,
                            const nsString& aHref, PRBool aAlternate,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);
  nsresult ProcessStyleLink(nsIContent* aElement,
                            const nsString& aHref, PRBool aAlternate,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);

  nsresult ProcessXSLStyleLink(nsIContent* aElement,
                            const nsString& aHref, PRBool aAlternate,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);
  nsresult CreateStyleSheetURL(nsIURI** aUrl, const nsAReadableString& aHref);
  nsresult LoadXSLStyleSheet(nsIURI* aUrl, const nsString& aType);
  nsresult SetupTransformMediator();
  nsresult AddText(const nsAReadableString& aString);

  static void
  GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult);

  void ScrollToRef();

  static nsINameSpaceManager* gNameSpaceManager;
  static PRUint32 gRefCnt;

  nsIDocument* mDocument;
  nsIURI* mDocumentURL;
  nsIURI* mDocumentBaseURL; // can be set via HTTP headers
  nsIWebShell* mWebShell;
  nsIParser* mParser;

  nsIContent* mRootElement;
  nsIContent* mDocElement;
  XMLContentSinkState mState;

  nsCOMPtr<nsISupportsArray> mContentStack;
  nsAutoVoidArray* mNameSpaceStack;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;
  PRPackedBool mConstrainSize;
  PRPackedBool mInTitle;

  PRPackedBool mNeedToBlockParser;
  PRUint32 mScriptLineNo;
  nsSupportsArray mScriptElements;

  nsString mStyleText;
  nsString  mPreferredStyle;
  PRInt32 mStyleSheetCount;
  nsICSSLoader* mCSSLoader;
  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;
  nsCOMPtr<nsITransformMediator> mXSLTransformMediator;

  nsString mRef; // ScrollTo #ref
  nsString mTitleText; 
  nsString mTextareaText; 

  nsCOMPtr<nsIDOMHTMLTextAreaElement> mTextAreaElement;
  nsCOMPtr<nsIHTMLContent> mStyleElement;
  nsCOMPtr<nsIHTMLContent> mBaseElement;
  nsCOMPtr<nsIHTMLContent> mMetaElement;
  nsCOMPtr<nsIHTMLContent> mLinkElement;
};

#endif // nsXMLContentSink_h__
