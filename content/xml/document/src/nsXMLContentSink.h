/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsXMLContentSink_h__
#define nsXMLContentSink_h__

#include "nsContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsIExpatSink.h"
#include "nsIDocumentTransformer.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"


class nsIDocument;
class nsIURI;
class nsIContent;
class nsINodeInfo;
class nsIParser;
class nsINameSpace;
class nsIViewManager;

typedef enum {
  eXMLContentSinkState_InProlog,
  eXMLContentSinkState_InDocumentElement,
  eXMLContentSinkState_InEpilog
} XMLContentSinkState;

class nsXMLContentSink : public nsContentSink,
                         public nsIXMLContentSink,
                         public nsITransformObserver,
                         public nsIExpatSink
{
public:
  nsXMLContentSink();
  virtual ~nsXMLContentSink();

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURL,
                nsISupports* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIEXPATSINK

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(void);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);  
  virtual void FlushContent(PRBool aNotify) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);

  // nsITransformObserver
  NS_IMETHOD OnDocumentCreated(nsIDOMDocument *aResultDocument);
  NS_IMETHOD OnTransformDone(nsresult aResult, nsIDOMDocument *aResultDocument);
  static void SplitXMLName(const nsAFlatString& aString, nsIAtom **aPrefix,
                           nsIAtom **aTagName);

protected:
  void StartLayout();

  nsresult PushNameSpacesFrom(const PRUnichar** aAttributes);
  nsresult AddAttributes(const PRUnichar** aNode, nsIContent* aContent);
  nsresult AddText(const PRUnichar* aString, PRInt32 aLength);
  nsresult ProcessEndSCRIPTTag(nsIContent* aContent);

  virtual PRBool OnOpenContainer(const PRUnichar **aAtts, 
                                 PRUint32 aAttsCount, 
                                 PRInt32 aNameSpaceID, 
                                 nsIAtom* aTagName,
                                 PRUint32 aLineNumber) { return PR_TRUE; }
  virtual nsresult CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount,
                                 nsINodeInfo* aNodeInfo, PRUint32 aLineNumber,
                                 nsIContent** aResult, PRBool* aAppendContent);

  virtual nsresult CloseElement(nsIContent* aContent, PRBool* aAppendContent);

  virtual nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                             PRBool* aDidFlush=nsnull);

  nsresult AddContentAsLeaf(nsIContent *aContent);

  PRInt32 GetNameSpaceId(nsIAtom* aPrefix);
  already_AddRefed<nsINameSpace> PopNameSpaces();

  nsIContent* GetCurrentContent();
  PRInt32 PushContent(nsIContent *aContent);
  already_AddRefed<nsIContent> PopContent();


  nsresult ProcessBASETag(nsIContent* aContent);

  
  // nsContentSink override
  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsAString& aHref,
                                    PRBool aAlternate,
                                    const nsAString& aTitle,
                                    const nsAString& aType,
                                    const nsAString& aMedia);

  nsresult LoadXSLStyleSheet(nsIURI* aUrl);

  nsresult MaybePrettyPrint();
  
  nsIContent*      mDocElement;
  PRUnichar*       mText;

  XMLContentSinkState mState;

  nsString mTitleText; 
  
  PRInt32 mTextLength;
  PRInt32 mTextSize;
  PRUint32 mScriptLineNo;
  
  PRUint8 mConstrainSize : 1;
  PRUint8 mInTitle : 1;
  PRUint8 mPrettyPrintXML : 1;
  PRUint8 mPrettyPrintHasSpecialRoot : 1;
  PRUint8 mPrettyPrintHasFactoredElements : 1;
  PRUint8 mHasProcessedBase : 1;
  PRUint8 mAllowAutoXLinks : 1;
  PRUint8 unused : 1;  // bit available if someone needs one
  
  nsCOMArray<nsIContent>           mContentStack;
  nsCOMArray<nsINameSpace>         mNameSpaceStack;
  nsCOMPtr<nsIDocumentTransformer> mXSLTProcessor;
};

#endif // nsXMLContentSink_h__
