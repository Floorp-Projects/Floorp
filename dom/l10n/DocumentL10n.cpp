/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentL10n.h"
#include "nsIContentSink.h"
#include "mozilla/dom/DocumentL10nBinding.h"

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

/* static */
RefPtr<DocumentL10n> DocumentL10n::Create(Document* aDocument) {
  RefPtr<DocumentL10n> l10n = new DocumentL10n(aDocument);

  if (!l10n->Init()) {
    return nullptr;
  }
  return l10n.forget();
}

DocumentL10n::DocumentL10n(Document* aDocument)
    : DOMLocalization(aDocument->GetScopeObject()),
      mDocument(aDocument),
      mState(DocumentL10nState::Uninitialized) {
  mContentSink = do_QueryInterface(aDocument->GetCurrentContentSink());
}

bool DocumentL10n::Init() {
  ErrorResult rv;
  mReady = Promise::Create(mGlobal, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }
  return true;
}

void DocumentL10n::Activate(const bool aLazy) {
  if (mState > DocumentL10nState::Uninitialized) {
    return;
  }

  Element* elem = mDocument->GetDocumentElement();
  if (NS_WARN_IF(!elem)) {
    return;
  }
  bool isSync = elem->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nsync);

  if (aLazy) {
    if (isSync) {
      NS_WARNING(
          "Document localization initialized lazy, data-l10n-sync attribute "
          "has no effect.");
    }
    DOMLocalization::Activate(false, true, {});
  } else {
    DOMLocalization::Activate(isSync, true, {});
  }
  mState = DocumentL10nState::Activated;
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

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    mDocumentL10n->InitialTranslationCompleted();
    mPromise->MaybeResolveWithUndefined();
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    mDocumentL10n->InitialTranslationCompleted();
    mPromise->MaybeRejectWithUndefined();
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
  if (mState >= DocumentL10nState::InitialTranslationTriggered) {
    return;
  }

  nsTArray<RefPtr<Promise>> promises;

  ErrorResult rv;
  promises.AppendElement(TranslateDocument(rv));
  if (NS_WARN_IF(rv.Failed())) {
    InitialTranslationCompleted();
    mReady->MaybeRejectWithUndefined();
    return;
  }
  promises.AppendElement(TranslateRoots(rv));
  Element* documentElement = mDocument->GetDocumentElement();
  if (!documentElement) {
    InitialTranslationCompleted();
    mReady->MaybeRejectWithUndefined();
    return;
  }

  DOMLocalization::ConnectRoot(*documentElement, rv);
  if (NS_WARN_IF(rv.Failed())) {
    InitialTranslationCompleted();
    mReady->MaybeRejectWithUndefined();
    return;
  }

  AutoEntryScript aes(mGlobal, "DocumentL10n InitialTranslation");
  RefPtr<Promise> promise = Promise::All(aes.cx(), promises, rv);

  if (promise->State() == Promise::PromiseState::Resolved) {
    // If the promise is already resolved, we can fast-track
    // to initial translation completed.
    InitialTranslationCompleted();
    mReady->MaybeResolveWithUndefined();
  } else {
    RefPtr<PromiseNativeHandler> l10nReadyHandler =
        new L10nReadyHandler(mReady, this);
    promise->AppendNativeHandler(l10nReadyHandler);

    mState = DocumentL10nState::InitialTranslationTriggered;
  }
}

already_AddRefed<Promise> DocumentL10n::TranslateDocument(ErrorResult& aRv) {
  MOZ_ASSERT(mState == DocumentL10nState::Activated,
             "This method should be called only from Activated state.");
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

void DocumentL10n::InitialTranslationCompleted() {
  if (mState >= DocumentL10nState::Ready) {
    return;
  }

  Element* documentElement = mDocument->GetDocumentElement();
  if (documentElement) {
    SetRootInfo(documentElement);
  }

  mState = DocumentL10nState::Ready;

  mDocument->InitialTranslationCompleted();

  // In XUL scenario contentSink is nullptr.
  if (mContentSink) {
    mContentSink->InitialTranslationCompleted();
  }

  // If sync was true, we want to change the state of
  // mozILocalization to async now.
  if (mIsSync) {
    mIsSync = false;

    mLocalization->SetIsSync(mIsSync);
  }
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
