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
 * The Original Code is mozilla.org code.
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
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIHTMLContentContainer.h"
#include "nsIParser.h"
#include "jsapi.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "pldhash.h"
#include "nsIHttpChannel.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLStyleSheet.h"

// Document.Write() related
#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

#include "nsICommandManager.h"

class nsIParser;
class nsICSSLoader;
class nsIURI;
class nsIMarkupDocumentViewer;
class nsIDocumentCharsetInfo;
class nsICacheEntryDescriptor;

class nsHTMLDocument : public nsDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument,
                       public nsIDOMNSHTMLDocument,
                       public nsIHTMLContentContainer
{
public:
  nsHTMLDocument();
  virtual ~nsHTMLDocument();
  virtual nsresult Init();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup);

  NS_IMETHOD CreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager,
                         nsIStyleSet* aStyleSet,
                         nsIPresShell** aInstancePtrResult);

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE,
                               nsIContentSink* aSink = nsnull);

  NS_IMETHOD StopDocumentLoad();

  virtual void EndLoad();

  NS_IMETHOD AddImageMap(nsIDOMHTMLMapElement* aMap);

  NS_IMETHOD RemoveImageMap(nsIDOMHTMLMapElement* aMap);

  NS_IMETHOD GetImageMap(const nsAString& aMapName,
                         nsIDOMHTMLMapElement** aResult);

  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aStyleSheet);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aStyleSheet);
  NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

  virtual void GetBaseTarget(nsAString& aTarget) const;
  virtual void SetBaseTarget(const nsAString& aTarget);

  NS_IMETHOD SetReferrer(const nsAString& aReferrer);

  NS_IMETHOD GetCompatibilityMode(nsCompatibility& aMode);
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aMode);

  NS_IMETHOD_(PRBool) IsWriting()
  {
    return mWriteLevel != PRUint32(0);
  }

  virtual void ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);
  virtual void AttributeChanged(nsIContent* aChild,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType);
  virtual void AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute);

  virtual void FlushPendingNotifications(PRBool aFlushReflows = PR_TRUE,
                                         PRBool aUpdateViews = PR_FALSE);

  NS_IMETHOD_(PRBool) IsCaseSensitive();

  // nsIDOMDocument interface
  NS_DECL_NSIDOMDOCUMENT

  // nsIDOMNode interface
  NS_DECL_NSIDOMNODE

  // nsIDOM3Node interface
  NS_IMETHOD GetBaseURI(nsAString& aBaseURI);

  // nsIDOMHTMLDocument interface
  NS_IMETHOD GetTitle(nsAString & aTitle);
  NS_IMETHOD SetTitle(const nsAString & aTitle);
  NS_IMETHOD GetReferrer(nsAString & aReferrer);
  NS_IMETHOD GetURL(nsAString & aURL);
  NS_IMETHOD GetBody(nsIDOMHTMLElement * *aBody);
  NS_IMETHOD SetBody(nsIDOMHTMLElement * aBody);
  NS_IMETHOD GetImages(nsIDOMHTMLCollection * *aImages);
  NS_IMETHOD GetApplets(nsIDOMHTMLCollection * *aApplets);
  NS_IMETHOD GetLinks(nsIDOMHTMLCollection * *aLinks);
  NS_IMETHOD GetForms(nsIDOMHTMLCollection * *aForms);
  NS_IMETHOD GetAnchors(nsIDOMHTMLCollection * *aAnchors);
  NS_IMETHOD GetCookie(nsAString & aCookie);
  NS_IMETHOD SetCookie(const nsAString & aCookie);
  NS_IMETHOD Open(void);
  NS_IMETHOD Close(void);
  NS_IMETHOD Write(const nsAString & text);
  NS_IMETHOD Writeln(const nsAString & text);
  NS_IMETHOD GetElementsByName(const nsAString & elementName,
                               nsIDOMNodeList **_retval);

  // nsIDOMNSHTMLDocument interface
  NS_DECL_NSIDOMNSHTMLDOCUMENT

  /*
   * Returns true if document.domain was set for this document
   */
  NS_IMETHOD WasDomainSet(PRBool* aDomainWasSet);

  NS_IMETHOD ResolveName(const nsAString& aName,
                         nsIDOMHTMLFormElement *aForm,
                         nsISupports **aResult);

  NS_IMETHOD GetFormControlElements(nsIDOMNodeList** aReturn);
  NS_IMETHOD AddedForm();
  NS_IMETHOD RemovedForm();
  NS_IMETHOD GetNumFormsSynchronous(PRInt32* aNumForms);

  PRBool IsXHTML()
  {
    return mDefaultNamespaceID == kNameSpaceID_XHTML;
  }

protected:
  nsresult GetPixelDimensions(nsIPresShell* aShell,
                              PRInt32* aWidth,
                              PRInt32* aHeight);

  nsresult RegisterNamedItems(nsIContent *aContent);
  nsresult UnregisterNamedItems(nsIContent *aContent);
  nsresult UpdateNameTableEntry(const nsAString& aName,
                                nsIContent *aContent);
  nsresult AddToIdTable(const nsAString& aId, nsIContent *aContent);
  nsresult UpdateIdTableEntry(const nsAString& aId, nsIContent *aContent);
  nsresult RemoveFromNameTable(const nsAString& aName, nsIContent *aContent);
  nsresult RemoveFromIdTable(nsIContent *aContent);

  void InvalidateHashTables();
  nsresult PrePopulateHashTables();

  nsIContent *MatchId(nsIContent *aContent, const nsAString& aId);

  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet,
                                     PRUint32 aFlags);
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet,
                                          PRInt32 aIndex);
  virtual nsIStyleSheet* InternalGetStyleSheetAt(PRInt32 aIndex) const;
  virtual PRInt32 InternalGetNumberOfStyleSheets() const;

  static PRBool MatchLinks(nsIContent *aContent, nsString* aData);
  static PRBool MatchAnchors(nsIContent *aContent, nsString* aData);
  static PRBool MatchLayers(nsIContent *aContent, nsString* aData);
  static PRBool MatchNameAttribute(nsIContent* aContent, nsString* aData);
  static PRBool MatchFormControls(nsIContent* aContent, nsString* aData);

  static nsresult GetSourceDocumentURL(nsIURI** sourceURL);

  static void DocumentWriteTerminationFunc(nsISupports *aRef);

  PRBool GetBodyContent();
  void GetBodyElement(nsIDOMHTMLBodyElement** aBody);

  void GetDomainURI(nsIURI **uri);

  nsresult WriteCommon(const nsAString& aText,
                       PRBool aNewlineTerminate);
  nsresult ScriptWriteCommon(PRBool aNewlineTerminate);
  nsresult OpenCommon(nsIURI* aUrl);

  nsresult CreateAndAddWyciwygChannel(void);
  nsresult RemoveWyciwygChannel(void);

  void BaseResetToURI(nsIURI* aURI);

  virtual void RetrieveRelevantHeaders(nsIChannel *aChannel);

  nsCOMPtr<nsIHTMLStyleSheet> mAttrStyleSheet;
  nsCOMPtr<nsIHTMLCSSStyleSheet> mStyleAttrStyleSheet;

  nsString mBaseTarget;
  nsString mReferrer;

  nsCOMPtr<nsIChannel>     mChannel;
  nsCOMPtr<nsIHttpChannel> mHttpChannel;

  nsCompatibility mCompatMode;

  nsCOMArray<nsIDOMHTMLMapElement> mImageMaps;

  nsCOMPtr<nsIDOMHTMLCollection> mImages;
  nsCOMPtr<nsIDOMHTMLCollection> mApplets;
  nsCOMPtr<nsIDOMHTMLCollection> mEmbeds;
  nsCOMPtr<nsIDOMHTMLCollection> mLinks;
  nsCOMPtr<nsIDOMHTMLCollection> mAnchors;
  nsCOMPtr<nsIDOMHTMLCollection> mForms;

  nsCOMPtr<nsIParser> mParser;

  /** # of forms in the document, synchronously set */
  PRInt32 mNumForms;

  // ahmed 12-2
  PRInt32  mTexttype;

  static nsrefcnt gRefCntRDFService;
  static nsIRDFService* gRDF;
  static PRUint32 gWyciwygSessionCnt;

  static PRBool TryHintCharset(nsIMarkupDocumentViewer* aMarkupDV,
                               PRInt32& aCharsetSource,
                               nsACString& aCharset);
  static PRBool TryUserForcedCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                     nsIDocumentCharsetInfo*  aDocInfo,
                                     PRInt32& aCharsetSource,
                                     nsACString& aCharset);
  static PRBool TryCacheCharset(nsICacheEntryDescriptor* aCacheDescriptor,
                                PRInt32& aCharsetSource,
                                nsACString& aCharset);
  static PRBool TryBookmarkCharset(nsIDocShell* aDocShell,
                                   nsIChannel* aChannel,
                                   PRInt32& aCharsetSource,
                                   nsACString& aCharset);
  static PRBool TryParentCharset(nsIDocumentCharsetInfo*  aDocInfo,
                                 PRInt32& charsetSource, nsACString& aCharset);
  static PRBool UseWeakDocTypeDefault(PRInt32& aCharsetSource,
                                      nsACString& aCharset);
  static PRBool TryChannelCharset(nsIChannel *aChannel,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);
  static PRBool TryDefaultCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);

  void StartAutodetection(nsIDocShell *aDocShell, nsACString& aCharset,
                          const char* aCommand);

  PRUint32 mIsWriting : 1;
  PRUint32 mWriteLevel : 31;

  // Load flags of the document's channel
  PRUint32 mLoadFlags;

  nsCOMPtr<nsIDOMNode> mBodyContent;

  /*
   * Bug 13871: Frameset spoofing - find out if document.domain was set
   */
  PRPackedBool mDomainWasSet;

  PLDHashTable mIdAndNameHashTable;

  nsCOMPtr<nsIWyciwygChannel> mWyciwygChannel;

  /* Midas implementation */
  nsresult   GetMidasCommandManager(nsICommandManager** aCommandManager);
  PRBool     ConvertToMidasInternalCommand(const nsAString & inCommandID,
                                           const nsAString & inParam,
                                           nsACString& outCommandID,
                                           nsACString& outParam,
                                           PRBool& isBoolean,
                                           PRBool& boolValue);
  nsCOMPtr<nsICommandManager> mMidasCommandManager;
  PRBool                      mEditingIsOn;

  nsresult   DoClipboardSecurityCheck(PRBool aPaste);
  static jsval       sCutCopyInternal_id;
  static jsval       sPasteInternal_id;

  // kNameSpaceID_None for good ol' HTML documents, and
  // kNameSpaceID_XHTML for spiffy new XHTML documents.
  PRInt32 mDefaultNamespaceID;
};

#endif /* nsHTMLDocument_h___ */
