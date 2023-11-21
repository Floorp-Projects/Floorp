/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "mozilla/Attributes.h"
#include "nsContentList.h"
#include "mozilla/dom/Document.h"
#include "nsIHTMLCollection.h"
#include "nsIScriptElement.h"
#include "nsTArray.h"

#include "PLDHashTable.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsCommandManager;
class nsIURI;
class nsIDocShell;
class nsICachingChannel;
class nsILoadGroup;

namespace mozilla::dom {
template <typename T>
struct Nullable;
class WindowProxyHolder;
}  // namespace mozilla::dom

class nsHTMLDocument : public mozilla::dom::Document {
 protected:
  using ReferrerPolicy = mozilla::dom::ReferrerPolicy;
  using Document = mozilla::dom::Document;
  using Encoding = mozilla::Encoding;
  template <typename T>
  using NotNull = mozilla::NotNull<T>;

 public:
  using Document::SetDocumentURI;

  nsHTMLDocument();
  virtual nsresult Init(nsIPrincipal* aPrincipal,
                        nsIPrincipal* aPartitionedPrincipal) override;

  // Document
  virtual void Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) override;
  virtual void ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                          nsIPrincipal* aPrincipal,
                          nsIPrincipal* aPartitionedPrincipal) override;

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset = true) override;

 protected:
  virtual bool UseWidthDeviceWidthFallbackViewport() const override;

 public:
  mozilla::dom::Element* GetUnfocusedKeyEventTarget() override;

  nsContentList* GetExistingForms() const { return mForms; }

  bool IsPlainText() const { return mIsPlainText; }

  bool IsViewSource() const { return mViewSource; }

  // Returns whether an object was found for aName.
  bool ResolveName(JSContext* aCx, const nsAString& aName,
                   JS::MutableHandle<JS::Value> aRetval,
                   mozilla::ErrorResult& aError);

  /**
   * Called when form->BindToTree() is called so that document knows
   * immediately when a form is added
   */
  void AddedForm();
  /**
   * Called when form->SetDocument() is called so that document knows
   * immediately when a form is removed
   */
  void RemovedForm();
  /**
   * Called to get a better count of forms than document.forms can provide
   * without calling FlushPendingNotifications (bug 138892).
   */
  // XXXbz is this still needed now that we can flush just content,
  // not the rest?
  int32_t GetNumFormsSynchronous() const;
  void SetIsXHTML(bool aXHTML) { mType = (aXHTML ? eXHTML : eHTML); }

  virtual nsresult Clone(mozilla::dom::NodeInfo*,
                         nsINode** aResult) const override;

  using mozilla::dom::DocumentOrShadowRoot::GetElementById;

  virtual void DocAddSizeOfExcludingThis(
      nsWindowSizes& aWindowSizes) const override;
  // DocAddSizeOfIncludingThis is inherited from Document.

  virtual bool WillIgnoreCharsetOverride() override;

  // WebIDL API
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
  bool IsRegistrableDomainSuffixOfOrEqualTo(const nsAString& aHostSuffixString,
                                            const nsACString& aOrigHost);
  void NamedGetter(JSContext* cx, const nsAString& aName, bool& aFound,
                   JS::MutableHandle<JSObject*> aRetval,
                   mozilla::ErrorResult& rv) {
    JS::Rooted<JS::Value> v(cx);
    if ((aFound = ResolveName(cx, aName, &v, rv))) {
      SetUseCounter(mozilla::eUseCounter_custom_HTMLDocumentNamedGetterHit);
      aRetval.set(v.toObjectOrNull());
    }
  }
  void GetSupportedNames(nsTArray<nsString>& aNames);
  // We're picking up GetLocation from Document
  already_AddRefed<mozilla::dom::Location> GetLocation() const {
    return Document::GetLocation();
  }

  static bool MatchFormControls(mozilla::dom::Element* aElement,
                                int32_t aNamespaceID, nsAtom* aAtom,
                                void* aData);

  void GetFormsAndFormControls(nsContentList** aFormList,
                               nsContentList** aFormControlList);

 protected:
  ~nsHTMLDocument();

  nsresult GetBodySize(int32_t* aWidth, int32_t* aHeight);

  nsIContent* MatchId(nsIContent* aContent, const nsAString& aId);

  static void DocumentWriteTerminationFunc(nsISupports* aRef);

  // A helper class to keep nsContentList objects alive for a short period of
  // time. Note, when the final Release is called on an nsContentList object, it
  // removes itself from MutationObserver list.
  class ContentListHolder : public mozilla::Runnable {
   public:
    ContentListHolder(nsHTMLDocument* aDocument, nsContentList* aFormList,
                      nsContentList* aFormControlList)
        : mozilla::Runnable("ContentListHolder"),
          mDocument(aDocument),
          mFormList(aFormList),
          mFormControlList(aFormControlList) {}

    ~ContentListHolder() {
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

  /** # of forms in the document, synchronously set */
  int32_t mNumForms;

  static void TryReloadCharset(nsIDocumentViewer* aViewer,
                               int32_t& aCharsetSource,
                               NotNull<const Encoding*>& aEncoding);
  void TryUserForcedCharset(nsIDocumentViewer* aViewer, nsIDocShell* aDocShell,
                            int32_t& aCharsetSource,
                            NotNull<const Encoding*>& aEncoding,
                            bool& aForceAutoDetection);
  void TryParentCharset(nsIDocShell* aDocShell, int32_t& charsetSource,
                        NotNull<const Encoding*>& aEncoding,
                        bool& aForceAutoDetection);

  // Load flags of the document's channel
  uint32_t mLoadFlags;

  bool mWarnedWidthHeight;

  /**
   * Set to true once we know that we are loading plain text content.
   */
  bool mIsPlainText;

  /**
   * Set to true once we know that we are viewing source.
   */
  bool mViewSource;
};

namespace mozilla::dom {

inline nsHTMLDocument* Document::AsHTMLDocument() {
  MOZ_ASSERT(IsHTMLOrXHTML());
  return static_cast<nsHTMLDocument*>(this);
}

inline const nsHTMLDocument* Document::AsHTMLDocument() const {
  MOZ_ASSERT(IsHTMLOrXHTML());
  return static_cast<const nsHTMLDocument*>(this);
}

}  // namespace mozilla::dom

#endif /* nsHTMLDocument_h___ */
