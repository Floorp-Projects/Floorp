/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptElement.h"
#include "jsapi.h"
#include "nsTArray.h"

#include "pldhash.h"
#include "nsIHttpChannel.h"
#include "nsHTMLStyleSheet.h"

// Document.Write() related
#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

#include "nsICommandManager.h"

class nsIEditor;
class nsIEditorDocShell;
class nsIParser;
class nsIURI;
class nsIMarkupDocumentViewer;
class nsIDocShell;
class nsICachingChannel;

class nsHTMLDocument : public nsDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument
{
public:
  using nsDocument::SetDocumentURI;
  using nsDocument::GetPlugins;

  nsHTMLDocument();
  virtual nsresult Init();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal);

  virtual nsresult CreateShell(nsPresContext* aContext,
                               nsIViewManager* aViewManager,
                               nsStyleSet* aStyleSet,
                               nsIPresShell** aInstancePtrResult);

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr);
  virtual void StopDocumentLoad();

  virtual void BeginLoad();

  virtual void EndLoad();

  virtual void SetCompatibilityMode(nsCompatibility aMode);

  virtual bool IsWriting()
  {
    return mWriteLevel != uint32_t(0);
  }

  virtual NS_HIDDEN_(nsContentList*) GetForms();
 
  virtual NS_HIDDEN_(nsContentList*) GetFormControls();
 
  // nsIDOMDocument interface
  NS_FORWARD_NSIDOMDOCUMENT(nsDocument::)

  // And explicitly import the things from nsDocument that we just shadowed
  using nsDocument::GetImplementation;
  using nsDocument::GetTitle;
  using nsDocument::SetTitle;
  using nsDocument::GetLastStyleSheetSet;
  using nsDocument::MozSetImageElement;
  using nsDocument::GetMozFullScreenElement;

  // nsIDOMNode interface
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMHTMLDocument interface
  NS_DECL_NSIDOMHTMLDOCUMENT

  /**
   * Returns the result of document.all[aID] which can either be a node
   * or a nodelist depending on if there are multiple nodes with the same
   * id.
   */
  nsISupports *GetDocumentAllResult(const nsAString& aID,
                                    nsWrapperCache **aCache,
                                    nsresult *aResult);

  Element *GetBody();
  Element *GetHead() { return GetHeadElement(); }
  already_AddRefed<nsContentList> GetElementsByName(const nsAString & aName)
  {
    return NS_GetFuncStringNodeList(this, MatchNameAttribute, nullptr,
                                    UseExistingNameString, aName);
  }

  virtual nsresult ResolveName(const nsAString& aName,
                               nsIContent *aForm,
                               nsISupports **aResult,
                               nsWrapperCache **aCache);

  virtual void AddedForm();
  virtual void RemovedForm();
  virtual int32_t GetNumFormsSynchronous();
  virtual void TearingDownEditor(nsIEditor *aEditor);
  virtual void SetIsXHTML(bool aXHTML) { mIsRegularHTML = !aXHTML; }
  virtual void SetDocWriteDisabled(bool aDisabled)
  {
    mDisableDocWrite = aDisabled;
  }

  nsresult ChangeContentEditableCount(nsIContent *aElement, int32_t aChange);
  void DeferredContentEditableCountChange(nsIContent *aElement);

  virtual EditingState GetEditingState()
  {
    return mEditingState;
  }

  virtual void DisableCookieAccess()
  {
    mDisableCookieAccess = true;
  }

  class nsAutoEditingState {
  public:
    nsAutoEditingState(nsHTMLDocument* aDoc, EditingState aState)
      : mDoc(aDoc), mSavedState(aDoc->mEditingState)
    {
      aDoc->mEditingState = aState;
    }
    ~nsAutoEditingState() {
      mDoc->mEditingState = mSavedState;
    }
  private:
    nsHTMLDocument* mDoc;
    EditingState    mSavedState;
  };
  friend class nsAutoEditingState;

  void EndUpdate(nsUpdateType aUpdateType);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLDocument, nsDocument)

  virtual nsresult SetEditingState(EditingState aState);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual NS_HIDDEN_(void) RemovedFromDocShell();

  virtual mozilla::dom::Element *GetElementById(const nsAString& aElementId)
  {
    return nsDocument::GetElementById(aElementId);
  }

  virtual nsXPCClassInfo* GetClassInfo();

  virtual void DocSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const;
  // DocSizeOfIncludingThis is inherited from nsIDocument.

protected:
  nsresult GetBodySize(int32_t* aWidth,
                       int32_t* aHeight);

  nsIContent *MatchId(nsIContent *aContent, const nsAString& aId);

  static bool MatchLinks(nsIContent *aContent, int32_t aNamespaceID,
                           nsIAtom* aAtom, void* aData);
  static bool MatchAnchors(nsIContent *aContent, int32_t aNamespaceID,
                             nsIAtom* aAtom, void* aData);
  static bool MatchNameAttribute(nsIContent* aContent, int32_t aNamespaceID,
                                   nsIAtom* aAtom, void* aData);
  static void* UseExistingNameString(nsINode* aRootNode, const nsString* aName);

  static void DocumentWriteTerminationFunc(nsISupports *aRef);

  void GetDomainURI(nsIURI **uri);

  nsresult WriteCommon(JSContext *cx, const nsAString& aText,
                       bool aNewlineTerminate);

  nsresult CreateAndAddWyciwygChannel(void);
  nsresult RemoveWyciwygChannel(void);

  /**
   * Like IsEditingOn(), but will flush as needed first.
   */
  bool IsEditingOnAfterFlush();

  void *GenerateParserKey(void);

  nsCOMPtr<nsIDOMHTMLCollection> mImages;
  nsCOMPtr<nsIDOMHTMLCollection> mApplets;
  nsCOMPtr<nsIDOMHTMLCollection> mEmbeds;
  nsCOMPtr<nsIDOMHTMLCollection> mLinks;
  nsCOMPtr<nsIDOMHTMLCollection> mAnchors;
  nsCOMPtr<nsIDOMHTMLCollection> mScripts;
  nsRefPtr<nsContentList> mForms;
  nsRefPtr<nsContentList> mFormControls;

  /** # of forms in the document, synchronously set */
  int32_t mNumForms;

  static uint32_t gWyciwygSessionCnt;

  static bool IsAsciiCompatible(const nsACString& aPreferredName);

  static void TryHintCharset(nsIMarkupDocumentViewer* aMarkupDV,
                               int32_t& aCharsetSource,
                               nsACString& aCharset);
  static bool TryUserForcedCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                     nsIDocShell*  aDocShell,
                                     int32_t& aCharsetSource,
                                     nsACString& aCharset);
  static bool TryCacheCharset(nsICachingChannel* aCachingChannel,
                                int32_t& aCharsetSource,
                                nsACString& aCharset);
  // aParentDocument could be null.
  void TryParentCharset(nsIDocShell*  aDocShell,
                          nsIDocument* aParentDocument,
                          int32_t& charsetSource, nsACString& aCharset);
  static void UseWeakDocTypeDefault(int32_t& aCharsetSource,
                                      nsACString& aCharset);
  static bool TryDefaultCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                  int32_t& aCharsetSource,
                                  nsACString& aCharset);

  // Override so we can munge the charset on our wyciwyg channel as needed.
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID);

  // Tracks if we are currently processing any document.write calls (either
  // implicit or explicit). Note that if a write call writes out something which
  // would block the parser, then mWriteLevel will be incorrect until the parser
  // finishes processing that script.
  uint32_t mWriteLevel;

  // Load flags of the document's channel
  uint32_t mLoadFlags;

  bool mTooDeepWriteRecursion;

  bool mDisableDocWrite;

  bool mWarnedWidthHeight;

  nsCOMPtr<nsIWyciwygChannel> mWyciwygChannel;

  /* Midas implementation */
  nsresult   GetMidasCommandManager(nsICommandManager** aCommandManager);

  nsCOMPtr<nsICommandManager> mMidasCommandManager;

  nsresult TurnEditingOff();
  nsresult EditingStateChanged();
  void MaybeEditingStateChanged();

  uint32_t mContentEditableCount;
  EditingState mEditingState;

  nsresult   DoClipboardSecurityCheck(bool aPaste);
  static jsid        sCutCopyInternal_id;
  static jsid        sPasteInternal_id;

  // When false, the .cookies property is completely disabled
  bool mDisableCookieAccess;
};

#define NS_HTML_DOCUMENT_INTERFACE_TABLE_BEGIN(_class)                        \
    NS_DOCUMENT_INTERFACE_TABLE_BEGIN(_class)                                 \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIHTMLDocument)                         \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIDOMHTMLDocument)

#endif /* nsHTMLDocument_h___ */
