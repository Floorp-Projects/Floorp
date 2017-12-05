/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "mozilla/Attributes.h"
#include "nsDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIHTMLCollection.h"
#include "nsIScriptElement.h"
#include "nsTArray.h"

#include "PLDHashTable.h"
#include "nsIHttpChannel.h"
#include "nsHTMLStyleSheet.h"
#include "nsThreadUtils.h"
#include "nsICommandManager.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsIURI;
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
  virtual nsresult Init() override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLDocument, nsDocument)

  // nsIDocument
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) override;
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal) override;

  virtual nsresult StartDocumentLoad(const char* aCommand,
                                     nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener **aDocListener,
                                     bool aReset = true,
                                     nsIContentSink* aSink = nullptr) override;
  virtual void StopDocumentLoad() override;

  virtual void BeginLoad() override;
  virtual void EndLoad() override;

  // nsIHTMLDocument
  virtual void SetCompatibilityMode(nsCompatibility aMode) override;

  virtual bool IsWriting() override
  {
    return mWriteLevel != uint32_t(0);
  }

  virtual nsIContent* GetUnfocusedKeyEventTarget() override;

  nsContentList* GetForms();

  nsContentList* GetExistingForms() const
  {
    return mForms;
  }

  // nsIDOMDocument interface
  using nsDocument::CreateElement;
  using nsDocument::CreateElementNS;
  NS_FORWARD_NSIDOMDOCUMENT(nsDocument::)

  // And explicitly import the things from nsDocument that we just shadowed
  using nsDocument::GetImplementation;
  using nsDocument::GetTitle;
  using nsDocument::SetTitle;
  using nsDocument::GetLastStyleSheetSet;
  using nsDocument::MozSetImageElement;

  // nsIDOMNode interface
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMHTMLDocument interface
  NS_DECL_NSIDOMHTMLDOCUMENT

  mozilla::dom::HTMLAllCollection* All();

  nsISupports* ResolveName(const nsAString& aName, nsWrapperCache **aCache);

  virtual void AddedForm() override;
  virtual void RemovedForm() override;
  virtual int32_t GetNumFormsSynchronous() override;
  virtual void TearingDownEditor() override;
  virtual void SetIsXHTML(bool aXHTML) override
  {
    mType = (aXHTML ? eXHTML : eHTML);
  }
  virtual void SetDocWriteDisabled(bool aDisabled) override
  {
    mDisableDocWrite = aDisabled;
  }

  nsresult ChangeContentEditableCount(nsIContent *aElement, int32_t aChange) override;
  void DeferredContentEditableCountChange(nsIContent *aElement);

  virtual EditingState GetEditingState() override
  {
    return mEditingState;
  }

  virtual void DisableCookieAccess() override
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

  void EndUpdate(nsUpdateType aUpdateType) override;

  virtual void SetMayStartLayout(bool aMayStartLayout) override;

  virtual nsresult SetEditingState(EditingState aState) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  virtual void RemovedFromDocShell() override;

  virtual mozilla::dom::Element *GetElementById(const nsAString& aElementId) override
  {
    return nsDocument::GetElementById(aElementId);
  }

  virtual void DocAddSizeOfExcludingThis(nsWindowSizes& aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from nsIDocument.

  virtual bool WillIgnoreCharsetOverride() override;

  // WebIDL API
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
    override;
  void SetDomain(const nsAString& aDomain, mozilla::ErrorResult& rv);
  bool IsRegistrableDomainSuffixOfOrEqualTo(const nsAString& aHostSuffixString,
                                            const nsACString& aOrigHost);
  void GetCookie(nsAString& aCookie, mozilla::ErrorResult& rv);
  void SetCookie(const nsAString& aCookie, mozilla::ErrorResult& rv);
  void NamedGetter(JSContext* cx, const nsAString& aName, bool& aFound,
                   JS::MutableHandle<JSObject*> aRetval,
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
  already_AddRefed<nsIDocument> Open(JSContext* cx,
                                     const nsAString& aType,
                                     const nsAString& aReplace,
                                     mozilla::ErrorResult& aError);
  already_AddRefed<nsPIDOMWindowOuter>
  Open(JSContext* cx,
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
  void GetDesignMode(nsAString& aDesignMode,
                     nsIPrincipal& aSubjectPrincipal)
  {
    GetDesignMode(aDesignMode);
  }
  void SetDesignMode(const nsAString& aDesignMode,
                     nsIPrincipal& aSubjectPrincipal,
                     mozilla::ErrorResult& rv);
  void SetDesignMode(const nsAString& aDesignMode,
                     const mozilla::Maybe<nsIPrincipal*>& aSubjectPrincipal,
                     mozilla::ErrorResult& rv);
  bool ExecCommand(const nsAString& aCommandID, bool aDoShowUI,
                   const nsAString& aValue,
                   nsIPrincipal& aSubjectPrincipal,
                   mozilla::ErrorResult& rv);
  bool QueryCommandEnabled(const nsAString& aCommandID,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& rv);
  bool QueryCommandIndeterm(const nsAString& aCommandID,
                            mozilla::ErrorResult& rv);
  bool QueryCommandState(const nsAString& aCommandID, mozilla::ErrorResult& rv);
  bool QueryCommandSupported(const nsAString& aCommandID,
                             mozilla::dom::CallerType aCallerType);
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
  // The XPCOM CaptureEvents works fine for us.
  // The XPCOM ReleaseEvents works fine for us.
  // We're picking up GetLocation from Document
  already_AddRefed<mozilla::dom::Location> GetLocation() const
  {
    return nsIDocument::GetLocation();
  }

  virtual nsHTMLDocument* AsHTMLDocument() override { return this; }

  static bool MatchFormControls(Element* aElement, int32_t aNamespaceID,
                                nsAtom* aAtom, void* aData);

  void GetFormsAndFormControls(nsContentList** aFormList,
                               nsContentList** aFormControlList);
protected:
  ~nsHTMLDocument();

  nsresult GetBodySize(int32_t* aWidth,
                       int32_t* aHeight);

  nsIContent *MatchId(nsIContent *aContent, const nsAString& aId);

  static bool MatchLinks(mozilla::dom::Element* aElement, int32_t aNamespaceID,
                         nsAtom* aAtom, void* aData);
  static bool MatchAnchors(mozilla::dom::Element* aElement, int32_t aNamespaceID,
                           nsAtom* aAtom, void* aData);

  static void DocumentWriteTerminationFunc(nsISupports *aRef);

  already_AddRefed<nsIURI> GetDomainURI();
  already_AddRefed<nsIURI> CreateInheritingURIForHost(const nsACString& aHostString);
  already_AddRefed<nsIURI> RegistrableDomainSuffixOfInternal(const nsAString& aHostSuffixString,
                                                             nsIURI* aOrigHost);


  nsresult WriteCommon(JSContext *cx, const nsAString& aText,
                       bool aNewlineTerminate);
  // A version of WriteCommon used by WebIDL bindings
  void WriteCommon(JSContext *cx,
                   const mozilla::dom::Sequence<nsString>& aText,
                   bool aNewlineTerminate,
                   mozilla::ErrorResult& rv);

  nsresult CreateAndAddWyciwygChannel(void);
  nsresult RemoveWyciwygChannel(void);

  // This should *ONLY* be used in GetCookie/SetCookie.
  already_AddRefed<nsIChannel> CreateDummyChannelForCookies(nsIURI* aCodebaseURI);

  /**
   * Like IsEditingOn(), but will flush as needed first.
   */
  bool IsEditingOnAfterFlush();

  void *GenerateParserKey(void);

  // A helper class to keep nsContentList objects alive for a short period of
  // time. Note, when the final Release is called on an nsContentList object, it
  // removes itself from MutationObserver list.
  class ContentListHolder : public mozilla::Runnable
  {
  public:
    ContentListHolder(nsHTMLDocument* aDocument,
                      nsContentList* aFormList,
                      nsContentList* aFormControlList)
      : mozilla::Runnable("ContentListHolder")
      , mDocument(aDocument)
      , mFormList(aFormList)
      , mFormControlList(aFormControlList)
    {
    }

    ~ContentListHolder()
    {
      MOZ_ASSERT(!mDocument->mContentListHolder ||
                 mDocument->mContentListHolder == this);
      mDocument->mContentListHolder = nullptr;
    }

    RefPtr<nsHTMLDocument> mDocument;
    RefPtr<nsContentList> mFormList;
    RefPtr<nsContentList> mFormControlList;
  };

  friend class ContentListHolder;
  ContentListHolder* mContentListHolder;

  RefPtr<nsContentList> mImages;
  RefPtr<nsEmptyContentList> mApplets;
  RefPtr<nsContentList> mEmbeds;
  RefPtr<nsContentList> mLinks;
  RefPtr<nsContentList> mAnchors;
  RefPtr<nsContentList> mScripts;
  RefPtr<nsContentList> mForms;

  RefPtr<mozilla::dom::HTMLAllCollection> mAll;

  /** # of forms in the document, synchronously set */
  int32_t mNumForms;

  static uint32_t gWyciwygSessionCnt;

  static void TryHintCharset(nsIContentViewer* aContentViewer,
                             int32_t& aCharsetSource,
                             NotNull<const Encoding*>& aEncoding);
  void TryUserForcedCharset(nsIContentViewer* aCv,
                            nsIDocShell*  aDocShell,
                            int32_t& aCharsetSource,
                            NotNull<const Encoding*>& aEncoding);
  static void TryCacheCharset(nsICachingChannel* aCachingChannel,
                              int32_t& aCharsetSource,
                              NotNull<const Encoding*>& aEncoding);
  void TryParentCharset(nsIDocShell*  aDocShell,
                        int32_t& charsetSource,
                        NotNull<const Encoding*>& aEncoding);
  void TryTLD(int32_t& aCharsetSource, NotNull<const Encoding*>& aCharset);
  void TryFallback(int32_t& aCharsetSource,
                   NotNull<const Encoding*>& aEncoding);

  // Override so we can munge the charset on our wyciwyg channel as needed.
  virtual void
    SetDocumentCharacterSet(NotNull<const Encoding*> aEncoding) override;

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
