/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentL10n.h"
#include "nsIContentSink.h"
#include "nsContentUtils.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentL10nBinding.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::intl;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(DocumentL10n)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocumentL10n, DOMLocalization)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReady)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContentSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DocumentL10n, DOMLocalization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReady)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContentSink)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DocumentL10n, DOMLocalization)
NS_IMPL_RELEASE_INHERITED(DocumentL10n, DOMLocalization)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DocumentL10n)
NS_INTERFACE_MAP_END_INHERITING(DOMLocalization)

bool DocumentL10n::mIsFirstBrowserWindow = true;

/* static */
RefPtr<DocumentL10n> DocumentL10n::Create(Document* aDocument, bool aSync) {
  RefPtr<DocumentL10n> l10n = new DocumentL10n(aDocument, aSync);

  IgnoredErrorResult rv;
  l10n->mReady = Promise::Create(l10n->mGlobal, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return nullptr;
  }

  return l10n.forget();
}

DocumentL10n::DocumentL10n(Document* aDocument, bool aSync)
    : DOMLocalization(aDocument->GetScopeObject(), aSync),
      mDocument(aDocument),
      mState(DocumentL10nState::Constructed) {
  mContentSink = do_QueryInterface(aDocument->GetCurrentContentSink());
}

JSObject* DocumentL10n::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return DocumentL10n_Binding::Wrap(aCx, this, aGivenProto);
}

class L10nReadyHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(L10nReadyHandler)

  explicit L10nReadyHandler(Promise* aPromise, DocumentL10n* aDocumentL10n)
      : mPromise(aPromise), mDocumentL10n(aDocumentL10n) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mDocumentL10n->InitialTranslationCompleted(true);
    mPromise->MaybeResolveWithUndefined();
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    mDocumentL10n->InitialTranslationCompleted(false);

    nsTArray<nsCString> errors{
        "[dom/l10n] Could not complete initial document translation."_ns,
    };
    IgnoredErrorResult rv;
    MaybeReportErrorsToGecko(errors, rv, mDocumentL10n->GetParentObject());

    /**
     * We resolve the mReady here even if we encountered failures, because
     * there is nothing actionable for the user pending on `mReady` to do here
     * and we don't want to incentivized consumers of this API to plan the
     * same pending operation for resolve and reject scenario.
     *
     * Additionally, without it, the stderr received "uncaught promise
     * rejection" warning, which is noisy and not-actionable.
     *
     * So instead, we just resolve and report errors.
     */
    mPromise->MaybeResolveWithUndefined();
  }

 private:
  ~L10nReadyHandler() = default;

  RefPtr<Promise> mPromise;
  RefPtr<DocumentL10n> mDocumentL10n;
};

NS_IMPL_CYCLE_COLLECTION(L10nReadyHandler, mPromise, mDocumentL10n)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(L10nReadyHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(L10nReadyHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(L10nReadyHandler)

void DocumentL10n::TriggerInitialTranslation() {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
  if (mState >= DocumentL10nState::InitialTranslationTriggered) {
    return;
  }
  if (!mReady) {
    // If we don't have `mReady` it means that we are in shutdown mode.
    // See bug 1687118 for details.
    InitialTranslationCompleted(false);
    return;
  }

  mInitialTranslationStart = mozilla::TimeStamp::Now();

  AutoAllowLegacyScriptExecution exemption;

  nsTArray<RefPtr<Promise>> promises;

  ErrorResult rv;
  promises.AppendElement(TranslateDocument(rv));
  if (NS_WARN_IF(rv.Failed())) {
    InitialTranslationCompleted(false);
    mReady->MaybeRejectWithUndefined();
    return;
  }
  promises.AppendElement(TranslateRoots(rv));
  Element* documentElement = mDocument->GetDocumentElement();
  if (!documentElement) {
    InitialTranslationCompleted(false);
    mReady->MaybeRejectWithUndefined();
    return;
  }

  DOMLocalization::ConnectRoot(*documentElement, rv);
  if (NS_WARN_IF(rv.Failed())) {
    InitialTranslationCompleted(false);
    mReady->MaybeRejectWithUndefined();
    return;
  }

  AutoEntryScript aes(mGlobal, "DocumentL10n InitialTranslation");
  RefPtr<Promise> promise = Promise::All(aes.cx(), promises, rv);

  if (promise->State() == Promise::PromiseState::Resolved) {
    // If the promise is already resolved, we can fast-track
    // to initial translation completed.
    InitialTranslationCompleted(true);
    mReady->MaybeResolveWithUndefined();
  } else {
    RefPtr<PromiseNativeHandler> l10nReadyHandler =
        new L10nReadyHandler(mReady, this);
    promise->AppendNativeHandler(l10nReadyHandler);

    mState = DocumentL10nState::InitialTranslationTriggered;
  }
}

already_AddRefed<Promise> DocumentL10n::TranslateDocument(ErrorResult& aRv) {
  MOZ_ASSERT(mState == DocumentL10nState::Constructed,
             "This method should be called only from Constructed state.");
  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);

  Element* elem = mDocument->GetDocumentElement();
  if (!elem) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  // 1. Collect all localizable elements.
  Sequence<OwningNonNull<Element>> elements;
  GetTranslatables(*elem, elements, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  RefPtr<nsXULPrototypeDocument> proto = mDocument->GetPrototype();

  // 2. Check if the document has a prototype that may cache
  //    translated elements.
  if (proto) {
    // 2.1. Handle the case when we have proto.

    // 2.1.1. Move elements that are not in the proto to a separate
    //        array.
    Sequence<OwningNonNull<Element>> nonProtoElements;

    uint32_t i = elements.Length();
    while (i > 0) {
      Element* elem = elements.ElementAt(i - 1);
      MOZ_RELEASE_ASSERT(elem->HasAttr(nsGkAtoms::datal10nid));
      if (!elem->HasElementCreatedFromPrototypeAndHasUnmodifiedL10n()) {
        if (NS_WARN_IF(!nonProtoElements.AppendElement(*elem, fallible))) {
          promise->MaybeRejectWithUndefined();
          return promise.forget();
        }
        elements.RemoveElement(elem);
      }
      i--;
    }

    // We populate the sequence in reverse order. Let's bring it
    // back to top->bottom one.
    nonProtoElements.Reverse();

    AutoAllowLegacyScriptExecution exemption;

    nsTArray<RefPtr<Promise>> promises;

    // 2.1.2. If we're not loading from cache, push the elements that
    //        are in the prototype to be translated and cached.
    if (!proto->WasL10nCached() && !elements.IsEmpty()) {
      RefPtr<Promise> translatePromise =
          TranslateElements(elements, proto, aRv);
      if (NS_WARN_IF(!translatePromise || aRv.Failed())) {
        promise->MaybeRejectWithUndefined();
        return promise.forget();
      }
      promises.AppendElement(translatePromise);
    }

    // 2.1.3. If there are elements that are not in the prototype,
    //        localize them without attempting to cache and
    //        independently of if we're loading from cache.
    if (!nonProtoElements.IsEmpty()) {
      RefPtr<Promise> nonProtoTranslatePromise =
          TranslateElements(nonProtoElements, nullptr, aRv);
      if (NS_WARN_IF(!nonProtoTranslatePromise || aRv.Failed())) {
        promise->MaybeRejectWithUndefined();
        return promise.forget();
      }
      promises.AppendElement(nonProtoTranslatePromise);
    }

    // 2.1.4. Collect promises with Promise::All (maybe empty).
    AutoEntryScript aes(mGlobal, "DocumentL10n InitialTranslationCompleted");
    promise = Promise::All(aes.cx(), promises, aRv);
  } else {
    // 2.2. Handle the case when we don't have proto.

    // 2.2.1. Otherwise, translate all available elements,
    //        without attempting to cache them.
    promise = TranslateElements(elements, nullptr, aRv);
  }

  if (NS_WARN_IF(!promise || aRv.Failed())) {
    promise->MaybeRejectWithUndefined();
    return promise.forget();
  }

  return promise.forget();
}

void DocumentL10n::MaybeRecordTelemetry() {
  mozilla::TimeStamp initialTranslationEnd = mozilla::TimeStamp::Now();

  nsAutoString documentURI;
  ErrorResult rv;
  rv = mDocument->GetDocumentURI(documentURI);
  if (rv.Failed()) {
    return;
  }

  nsCString key;

  if (documentURI.Find(u"chrome://browser/content/browser.xhtml") == 0) {
    if (mIsFirstBrowserWindow) {
      key = "browser_first_window";
      mIsFirstBrowserWindow = false;
    } else {
      key = "browser_new_window";
    }
  } else if (documentURI.Find(u"about:home") == 0) {
    key = "about:home";
  } else if (documentURI.Find(u"about:newtab") == 0) {
    key = "about:newtab";
  } else if (documentURI.Find(u"about:preferences") == 0) {
    key = "about:preferences";
  } else {
    return;
  }

  mozilla::TimeDuration totalTime(initialTranslationEnd -
                                  mInitialTranslationStart);
  Accumulate(Telemetry::L10N_DOCUMENT_INITIAL_TRANSLATION_TIME_US, key,
             totalTime.ToMicroseconds());
}

void DocumentL10n::InitialTranslationCompleted(bool aL10nCached) {
  if (mState >= DocumentL10nState::Ready) {
    return;
  }

  Element* documentElement = mDocument->GetDocumentElement();
  if (documentElement) {
    SetRootInfo(documentElement);
  }

  mState = DocumentL10nState::Ready;

  MaybeRecordTelemetry();

  mDocument->InitialTranslationCompleted(aL10nCached);

  // In XUL scenario contentSink is nullptr.
  if (mContentSink) {
    mContentSink->InitialTranslationCompleted();
    mContentSink = nullptr;
  }

  // From now on, the state of Localization is unconditionally
  // async.
  SetAsync();
}

void DocumentL10n::ConnectRoot(nsINode& aNode, bool aTranslate,
                               ErrorResult& aRv) {
  if (aTranslate) {
    if (mState >= DocumentL10nState::InitialTranslationTriggered) {
      RefPtr<Promise> promise = TranslateFragment(aNode, aRv);
    }
  }
  DOMLocalization::ConnectRoot(aNode, aRv);
}

Promise* DocumentL10n::Ready() { return mReady; }

void DocumentL10n::OnCreatePresShell() { mMutations->OnCreatePresShell(); }
