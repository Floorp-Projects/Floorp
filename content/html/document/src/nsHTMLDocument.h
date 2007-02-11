/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * The Original Code is mozilla.org code.
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
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptElement.h"
#include "jsapi.h"

#include "pldhash.h"
#include "nsIHttpChannel.h"
#include "nsHTMLStyleSheet.h"

// Document.Write() related
#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

#include "nsICommandManager.h"

class nsIParser;
class nsIURI;
class nsIMarkupDocumentViewer;
class nsIDocumentCharsetInfo;
class nsICacheEntryDescriptor;

class nsHTMLDocument : public nsDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument,
                       public nsIDOMNSHTMLDocument
{
public:
  nsHTMLDocument();
  virtual ~nsHTMLDocument();
  virtual nsresult Init();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal);
  virtual nsStyleSet::sheetType GetAttrSheetType();

  virtual nsresult CreateShell(nsPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult);

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     PRBool aReset = PR_TRUE,
                                     nsIContentSink* aSink = nsnull);
  virtual void StopDocumentLoad();

  virtual void EndLoad();

  virtual nsresult AddImageMap(nsIDOMHTMLMapElement* aMap);

  virtual void RemoveImageMap(nsIDOMHTMLMapElement* aMap);

  virtual nsIDOMHTMLMapElement *GetImageMap(const nsAString& aMapName);

  virtual void SetCompatibilityMode(nsCompatibility aMode);

  virtual PRBool IsWriting()
  {
    return mWriteLevel != PRUint32(0);
  }

  virtual PRBool GetIsFrameset() { return mIsFrameset; }
  virtual void SetIsFrameset(PRBool aFrameset) { mIsFrameset = aFrameset; }

  virtual NS_HIDDEN_(nsContentList*) GetForms();
 
  virtual NS_HIDDEN_(nsContentList*) GetFormControls();
 
  virtual void AttributeWillChange(nsIContent* aChild,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute);

  virtual PRBool IsCaseSensitive();

  // nsIMutationObserver
  virtual void ContentAppended(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);
  virtual void AttributeChanged(nsIDocument* aDocument,
                                nsIContent* aChild,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType);
  // nsIDOMDocument interface
  NS_DECL_NSIDOMDOCUMENT

  // nsIDOM3Document interface
  NS_IMETHOD GetXmlEncoding(nsAString& aXmlVersion);
  NS_IMETHOD GetXmlStandalone(PRBool *aXmlStandalone);
  NS_IMETHOD SetXmlStandalone(PRBool aXmlStandalone);
  NS_IMETHOD GetXmlVersion(nsAString& aXmlVersion);
  NS_IMETHOD SetXmlVersion(const nsAString& aXmlVersion);

  // nsIDOMNode interface
  NS_FORWARD_NSIDOMNODE(nsDocument::)

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

  virtual nsresult ResolveName(const nsAString& aName,
                         nsIDOMHTMLFormElement *aForm,
                         nsISupports **aResult);

  virtual void ScriptLoading(nsIScriptElement *aScript);
  virtual void ScriptExecuted(nsIScriptElement *aScript);

  virtual void AddedForm();
  virtual void RemovedForm();
  virtual PRInt32 GetNumFormsSynchronous();

  PRBool IsXHTML()
  {
    return mDefaultNamespaceID == kNameSpaceID_XHTML;
  }

#ifdef DEBUG
  virtual nsresult CreateElem(nsIAtom *aName, nsIAtom *aPrefix,
                              PRInt32 aNamespaceID,
                              PRBool aDocumentDefaultType,
                              nsIContent** aResult);
#endif

protected:
  nsresult GetPixelDimensions(nsIPresShell* aShell,
                              PRInt32* aWidth,
                              PRInt32* aHeight);

  nsresult RegisterNamedItems(nsIContent *aContent);
  nsresult UnregisterNamedItems(nsIContent *aContent);
  nsresult UpdateNameTableEntry(nsIAtom* aName, nsIContent *aContent);
  nsresult UpdateIdTableEntry(nsIAtom* aId, nsIContent *aContent);
  nsresult RemoveFromNameTable(nsIAtom* aName, nsIContent *aContent);
  nsresult RemoveFromIdTable(nsIContent *aContent);

  void InvalidateHashTables();
  nsresult PrePopulateHashTables();

  nsIContent *MatchId(nsIContent *aContent, const nsAString& aId);

  static PRBool MatchLinks(nsIContent *aContent, PRInt32 aNamespaceID,
                           nsIAtom* aAtom, void* aData);
  static PRBool MatchAnchors(nsIContent *aContent, PRInt32 aNamespaceID,
                             nsIAtom* aAtom, void* aData);
  static PRBool MatchNameAttribute(nsIContent* aContent, PRInt32 aNamespaceID,
                                   nsIAtom* aAtom, void* aData);

  static void DocumentWriteTerminationFunc(nsISupports *aRef);

  PRBool GetBodyContent();
  void GetBodyElement(nsIDOMHTMLBodyElement** aBody);

  void GetDomainURI(nsIURI **uri);

  nsresult WriteCommon(const nsAString& aText,
                       PRBool aNewlineTerminate);
  nsresult ScriptWriteCommon(PRBool aNewlineTerminate);
  nsresult OpenCommon(const nsACString& aContentType, PRBool aReplace);

  nsresult CreateAndAddWyciwygChannel(void);
  nsresult RemoveWyciwygChannel(void);

  void *GenerateParserKey(void);

  PRInt32 GetDefaultNamespaceID() const
  {
    return mDefaultNamespaceID;
  };

  nsCOMArray<nsIDOMHTMLMapElement> mImageMaps;

  nsCOMPtr<nsIDOMHTMLCollection> mImages;
  nsCOMPtr<nsIDOMHTMLCollection> mApplets;
  nsCOMPtr<nsIDOMHTMLCollection> mEmbeds;
  nsCOMPtr<nsIDOMHTMLCollection> mLinks;
  nsCOMPtr<nsIDOMHTMLCollection> mAnchors;
  nsRefPtr<nsContentList> mForms;
  nsRefPtr<nsContentList> mFormControls;

  /** # of forms in the document, synchronously set */
  PRInt32 mNumForms;

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
  // aParentDocument could be null.
  PRBool TryParentCharset(nsIDocumentCharsetInfo*  aDocInfo,
                          nsIDocument* aParentDocument,
                          PRInt32& charsetSource, nsACString& aCharset);
  static PRBool UseWeakDocTypeDefault(PRInt32& aCharsetSource,
                                      nsACString& aCharset);
  static PRBool TryDefaultCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset);

  void StartAutodetection(nsIDocShell *aDocShell, nsACString& aCharset,
                          const char* aCommand);

  // mWriteState tracks the status of this document if the document is being
  // entirely created by script. In the normal load case, mWriteState will be
  // eNotWriting. Once document.open has been called (either implicitly or
  // explicitly), mWriteState will be eDocumentOpened. When document.close has
  // been called, mWriteState will become eDocumentClosed if there have been no
  // external script loads in the meantime. If there have been, then mWriteState
  // becomes ePendingClose, indicating that we might still be writing, but that
  // we shouldn't process any further close() calls.
  enum {
    eNotWriting,
    eDocumentOpened,
    ePendingClose,
    eDocumentClosed
  } mWriteState;

  // Tracks if we are currently processing any document.write calls (either
  // implicit or explicit). Note that if a write call writes out something which
  // would block the parser, then mWriteLevel will be incorrect until the parser
  // finishes processing that script.
  PRUint32 mWriteLevel;

  nsSmallVoidArray mPendingScripts;

  // Load flags of the document's channel
  PRUint32 mLoadFlags;

  nsCOMPtr<nsIDOMNode> mBodyContent;

  PRPackedBool mIsFrameset;

  PRPackedBool mTooDeepWriteRecursion;

  PRBool IdTableIsLive() const {
    // live if we've had over 63 misses
    return (mIdMissCount & 0x40) != 0;
  }

  PRBool IdTableShouldBecomeLive() {
    NS_ASSERTION(!IdTableIsLive(),
                 "Shouldn't be called if table is already live!");
    ++mIdMissCount;
    return IdTableIsLive();
  }

  PRUint8 mIdMissCount;

  /* mIdAndNameHashTable works as follows for IDs:
   * 1) Attribute changes affect the table immediately (removing and adding
   *    entries as needed).
   * 2) Removals from the DOM affect the table immediately
   * 3) Additions to the DOM always update existing entries, but only add new
   *    ones if IdTableIsLive() is true.
   */
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
  // XXXbz should this be reset if someone manually calls
  // SetContentType() on this document?
  PRInt32 mDefaultNamespaceID;
};

#endif /* nsHTMLDocument_h___ */
