/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *   IBM Corp.
 */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsMarkupDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIHTMLContentContainer.h"
#include "plhash.h"
#include "jsapi.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"

class nsContentList;
class nsIHTMLStyleSheet;
class nsIHTMLCSSStyleSheet;
class nsIParser;
class nsICSSLoader;

class nsHTMLDocument : public nsMarkupDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument,
                       public nsIDOMNSHTMLDocument,
                       public nsIHTMLContentContainer
{
public:
  nsHTMLDocument();
  virtual ~nsHTMLDocument();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD GetContentType(nsAWritableString& aContentType) const;

  NS_IMETHOD CreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager,
                         nsIStyleSet* aStyleSet,
                         nsIPresShell** aInstancePtrResult);

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE);

  NS_IMETHOD StopDocumentLoad();

  NS_IMETHOD EndLoad();

  NS_IMETHOD AddImageMap(nsIDOMHTMLMapElement* aMap);

  NS_IMETHOD RemoveImageMap(nsIDOMHTMLMapElement* aMap);

  NS_IMETHOD GetImageMap(const nsString& aMapName,
                         nsIDOMHTMLMapElement** aResult);

  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aStyleSheet);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aStyleSheet);
  NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

  NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;
  NS_IMETHOD SetBaseURL(const nsAReadableString& aURLSpec);
  NS_IMETHOD GetBaseTarget(nsAWritableString& aTarget) const;
  NS_IMETHOD SetBaseTarget(const nsAReadableString& aTarget);

  NS_IMETHOD SetLastModified(const nsAReadableString& aLastModified);
  NS_IMETHOD SetReferrer(const nsAReadableString& aReferrer);

  NS_IMETHOD GetDTDMode(nsDTDMode& aMode);
  NS_IMETHOD SetDTDMode(nsDTDMode aMode);

  NS_IMETHOD SetHeaderData(nsIAtom* aHeaderField,
                           const nsAReadableString& aData);

  NS_IMETHOD ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD FlushPendingNotifications(PRBool aFlushReflows = PR_TRUE);

  // nsIDOMDocument interface
  NS_DECL_IDOMDOCUMENT

  // nsIDOMNode interface
  NS_DECL_IDOMNODE

  // nsIDOMHTMLDocument interface
  NS_DECL_IDOMHTMLDOCUMENT
  NS_DECL_IDOMNSHTMLDOCUMENT
  // the following is not part of nsIDOMHTMLDOCUMENT but allows the content sink to add forms
  NS_IMETHOD AddForm(nsIDOMHTMLFormElement* aForm);

  // From nsIScriptObjectOwner interface, implemented by nsDocument
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  
  // From nsJSScriptObject interface, implemented by nsDocument
  virtual PRBool Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                         PRBool *aDidDefineProperty);

  virtual nsresult Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

  /*
   * Returns true if document.domain was set for this document
   */
  NS_IMETHOD WasDomainSet(PRBool* aDomainWasSet);

protected:
  nsresult GetPixelDimensions(nsIPresShell* aShell,
                              PRInt32* aWidth,
                              PRInt32* aHeight);

  void RegisterNamedItems(nsIContent *aContent, PRBool aInForm);
  void UnregisterNamedItems(nsIContent *aContent, PRBool aInForm);
  nsIContent* FindNamedItem(nsIContent *aContent, const nsString& aName,
                            PRBool aInForm);

  void DeleteNamedItems();
  nsIContent *MatchId(nsIContent *aContent, const nsAReadableString& aId);

  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet,
                                          PRInt32 aIndex);
  static PRBool MatchLinks(nsIContent *aContent, nsString* aData);
  static PRBool MatchAnchors(nsIContent *aContent, nsString* aData);
  static PRBool MatchLayers(nsIContent *aContent, nsString* aData);
  static PRBool MatchNameAttribute(nsIContent* aContent, nsString* aData);

  nsresult GetSourceDocumentURL(JSContext* cx, nsIURI** sourceURL);

  PRBool GetBodyContent();
  nsresult GetBodyElement(nsIDOMHTMLBodyElement** aBody);

  NS_IMETHOD GetDomainURI(nsIURI **uri);

  nsresult WriteCommon(const nsAReadableString& aText,
                       PRBool aNewlineTerminate);
  nsresult ScriptWriteCommon(JSContext *cx, 
                             jsval *argv, 
                             PRUint32 argc,
                             PRBool aNewlineTerminate);
  nsresult OpenCommon(nsIURI* aUrl);

  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mStyleAttrStyleSheet;
  nsIURI*     mBaseURL;
  nsString*   mBaseTarget;
  nsString*   mLastModified;
  nsString*   mReferrer;
  nsDTDMode mDTDMode;
  nsVoidArray mImageMaps;
  nsICSSLoader* mCSSLoader;

  nsContentList *mImages;
  nsContentList *mApplets;
  nsContentList *mEmbeds;
  nsContentList *mLinks;
  nsContentList *mAnchors;
  nsContentList *mForms;
  nsContentList *mLayers;
  
  PLHashTable *mNamedItems;

  nsIParser *mParser;
  
  static nsrefcnt gRefCntRDFService;
  static nsIRDFService* gRDF;

  PRUint32 mIsWriting : 1;
  PRUint32 mWriteLevel : 31;

  nsIDOMNode * mBodyContent;

  /*
   * Bug 13871: Frameset spoofing - find out if document.domain was set
   */
  PRBool       mDomainWasSet;

};

#endif /* nsHTMLDocument_h___ */
