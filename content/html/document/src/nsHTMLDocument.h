/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "mozilla/Attributes.h"
#include "nsDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptElement.h"
#include "nsTArray.h"

#include "pldhash.h"
#include "nsIHttpChannel.h"
#include "nsHTMLStyleSheet.h"

#include "nsICommandManager.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "nsDOMEvent.h"

class nsIEditor;
class nsIParser;
class nsIURI;
class nsIMarkupDocumentViewer;
class nsIDocShell;
class nsICachingChannel;
class nsIWyciwygChannel;
class nsILoadGroup;

namespace mozilla {
namespace dom {
class HTMLAllCollection;
} // namespace dom
} // namespace mozilla

class nsHTMLDocument : public nsDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument
{
public:
  using nsDocument::SetDocumentURI;
  using nsDocument::GetPlugins;

  nsHTMLDocument();
  ~nsHTMLDocument();
  virtual nsresult Init() MOZ_OVERRIDE;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLDocument, nsDocument)

  // nsIDocument
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) MOZ_OVERRIDE;
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIPresShell> CreateShell(nsPresContext* aContext,
                                                     nsViewManager* aViewManager,
                                                     nsStyleSet* aStyleSet) MOZ_OVERRIDE;

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr) MOZ_OVERRIDE;
  virtual void StopDocumentLoad() MOZ_OVERRIDE;

  virtual void BeginLoad() MOZ_OVERRIDE;
  virtual void EndLoad() MOZ_OVERRIDE;

  // nsIHTMLDocument
  virtual void SetCompatibilityMode(nsCompatibility aMode) MOZ_OVERRIDE;

  virtual bool IsWriting() MOZ_OVERRIDE
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
  JSObject* GetAll(JSContext* aCx, mozilla::ErrorResult& aRv);

  nsISupports* ResolveName(const nsAString& aName, nsWrapperCache **aCache);

  virtual void AddedForm() MOZ_OVERRIDE;
  virtual void RemovedForm() MOZ_OVERRIDE;
  virtual int32_t GetNumFormsSynchronous() MOZ_OVERRIDE;
  virtual void TearingDownEditor(nsIEditor *aEditor) MOZ_OVERRIDE;
  virtual void SetIsXHTML(bool aXHTML) MOZ_OVERRIDE { mIsRegularHTML = !aXHTML; }
  virtual void SetDocWriteDisabled(bool aDisabled) MOZ_OVERRIDE
  {
    mDisableDocWrite = aDisabled;
  }

  nsresult ChangeContentEditableCount(nsIContent *aElement, int32_t aChange) MOZ_OVERRIDE;
  void DeferredContentEditableCountChange(nsIContent *aElement);

  virtual EditingState GetEditingState() MOZ_OVERRIDE
  {
    return mEditingState;
  }

  virtual void DisableCookieAccess() MOZ_OVERRIDE
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

  void EndUpdate(nsUpdateType aUpdateType) MOZ_OVERRIDE;

  virtual nsresult SetEditingState(EditingState aState) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual NS_HIDDEN_(void) RemovedFromDocShell() MOZ_OVERRIDE;

  virtual mozilla::dom::Element *GetElementById(const nsAString& aElementId)
  {
    return nsDocument::GetElementById(aElementId);
  }

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const MOZ_OVERRIDE;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.

  virtual bool WillIgnoreCharsetOverride() MOZ_OVERRIDE;

  // WebIDL API
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
    MOZ_OVERRIDE;
  void GetDomain(nsAString& aDomain, mozilla::ErrorResult& rv);
  void SetDomain(const nsAString& aDomain, mozilla::ErrorResult& rv);
  void GetCookie(nsAString& aCookie, mozilla::ErrorResult& rv);
  void SetCookie(const nsAString& aCookie, mozilla::ErrorResult& rv);
  JSObject* NamedGetter(JSContext* cx, const nsAString& aName, bool& aFound,
                        mozilla::ErrorResult& rv);
  void GetSupportedNames(nsTArray<nsString>& aNames);
  nsGenericHTMLElement *GetBody();
  void SetBody(nsGenericHTMLElement* aBody, mozilla::ErrorResult& rv);
  mozilla::dom::HTMLSharedElement *GetHead() {
    return static_cast<mozilla::dom::HTMLSharedElement*>(GetHeadElement());
  }
  nsIHTMLCollection* Images();
  nsIHTMLCollection* Embeds();
  nsIHTMLCollection* Plugins();
  nsIHTMLCollection* Links();
  nsIHTMLCollection* Forms()
  {
    return nsHTMLDocument::GetForms();
  }
  nsIHTMLCollection* Scripts();
  already_AddRefed<nsContentList> GetElementsByName(const nsAString & aName)
  {
    return NS_GetFuncStringNodeList(this, MatchNameAttribute, nullptr,
                                    UseExistingNameString, aName);
  }
  already_AddRefed<nsINodeList> GetItems(const nsAString& aTypeNames);
  already_AddRefed<nsIDocument> Open(JSContext* cx,
                                     const nsAString& aType,
                                     const nsAString& aReplace,
                                     mozilla::ErrorResult& rv);
  already_AddRefed<nsIDOMWindow> Open(JSContext* cx,
                                      const nsAString& aURL,
                                      const nsAString& aName,
                                      const nsAString& aFeatures,
                                      bool aReplace,
                                      mozilla::ErrorResult& rv);
  void Close(mozilla::ErrorResult& rv);
  void Write(JSContext* cx, const mozilla::dom::Sequence<nsString>& aText,
             mozilla::ErrorResult& rv);
  void Writeln(JSContext* cx, const mozilla::dom::Sequence<nsString>& aText,
               mozilla::ErrorResult& rv);
  // The XPCOM GetDesignMode() works OK for us, since it never throws.
  void SetDesignMode(const nsAString& aDesignMode, mozilla::ErrorResult& rv);
  bool ExecCommand(const nsAString& aCommandID, bool aDoShowUI,
                   const nsAString& aValue, mozilla::ErrorResult& rv);
  bool QueryCommandEnabled(const nsAString& aCommandID,
                           mozilla::ErrorResult& rv);
  bool QueryCommandIndeterm(const nsAString& aCommandID,
                            mozilla::ErrorResult& rv);
  bool QueryCommandState(const nsAString& aCommandID, mozilla::ErrorResult& rv);
  bool QueryCommandSupported(const nsAString& aCommandID);
  void QueryCommandValue(const nsAString& aCommandID, nsAString& aValue,
                         mozilla::ErrorResult& rv);
  // The XPCOM Get/SetFgColor work OK for us, since they never throw.
  // The XPCOM Get/SetLinkColor work OK for us, since they never throw.
  // The XPCOM Get/SetVLinkColor work OK for us, since they never throw.
  // The XPCOM Get/SetALinkColor work OK for us, since they never throw.
  // The XPCOM Get/SetBgColor work OK for us, since they never throw.
  nsIHTMLCollection* Anchors();
  nsIHTMLCollection* Applets();
  void Clear() const
  {
    // Deprecated
  }
  already_AddRefed<nsISelection> GetSelection(mozilla::ErrorResult& rv);
  // The XPCOM CaptureEvents works fine for us.
  // The XPCOM ReleaseEvents works fine for us.
  // We're picking up GetLocation from Document
  already_AddRefed<nsIDOMLocation> GetLocation() const {
    return nsIDocument::GetLocation();
  }

  virtual nsHTMLDocument* AsHTMLDocument() MOZ_OVERRIDE { return this; }

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

  already_AddRefed<nsIURI> GetDomainURI();

  nsresult WriteCommon(JSContext *cx, const nsAString& aText,
                       bool aNewlineTerminate);
  // A version of WriteCommon used by WebIDL bindings
  void WriteCommon(JSContext *cx,
                   const mozilla::dom::Sequence<nsString>& aText,
                   bool aNewlineTerminate,
                   mozilla::ErrorResult& rv);

  nsresult CreateAndAddWyciwygChannel(void);
  nsresult RemoveWyciwygChannel(void);

  /**
   * Like IsEditingOn(), but will flush as needed first.
   */
  bool IsEditingOnAfterFlush();

  void *GenerateParserKey(void);

  nsRefPtr<nsContentList> mImages;
  nsRefPtr<nsContentList> mApplets;
  nsRefPtr<nsContentList> mEmbeds;
  nsRefPtr<nsContentList> mLinks;
  nsRefPtr<nsContentList> mAnchors;
  nsRefPtr<nsContentList> mScripts;
  nsRefPtr<nsContentList> mForms;
  nsRefPtr<nsContentList> mFormControls;

  nsRefPtr<mozilla::dom::HTMLAllCollection> mAll;

  /** # of forms in the document, synchronously set */
  int32_t mNumForms;

  static uint32_t gWyciwygSessionCnt;

  static void TryHintCharset(nsIMarkupDocumentViewer* aMarkupDV,
                             int32_t& aCharsetSource,
                             nsACString& aCharset);
  void TryUserForcedCharset(nsIMarkupDocumentViewer* aMarkupDV,
                            nsIDocShell*  aDocShell,
                            int32_t& aCharsetSource,
                            nsACString& aCharset);
  static void TryCacheCharset(nsICachingChannel* aCachingChannel,
                                int32_t& aCharsetSource,
                                nsACString& aCharset);
  // aParentDocument could be null.
  void TryParentCharset(nsIDocShell*  aDocShell,
                        nsIDocument* aParentDocument,
                        int32_t& charsetSource, nsACString& aCharset);
  static void TryWeakDocTypeDefault(int32_t& aCharsetSource,
                                    nsACString& aCharset);
  static void TryDefaultCharset(nsIMarkupDocumentViewer* aMarkupDV,
                                int32_t& aCharsetSource,
                                nsACString& aCharset);

  // Override so we can munge the charset on our wyciwyg channel as needed.
  virtual void SetDocumentCharacterSet(const nsACString& aCharSetID) MOZ_OVERRIDE;

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

  /**
   * Temporary flag that is set in EndUpdate() to ignore
   * MaybeEditingStateChanged() script runners from a nested scope.
   */
  bool mPendingMaybeEditingStateChanged;
};

#define NS_HTML_DOCUMENT_INTERFACE_TABLE_BEGIN(_class)                        \
    NS_DOCUMENT_INTERFACE_TABLE_BEGIN(_class)                                 \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIHTMLDocument)                         \
    NS_INTERFACE_TABLE_ENTRY(_class, nsIDOMHTMLDocument)

#endif /* nsHTMLDocument_h___ */
