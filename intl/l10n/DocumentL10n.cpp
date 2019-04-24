/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ForOfIterator.h"  // JS::ForOfIterator
#include "js/JSON.h"           // JS_ParseJSON
#include "mozilla/dom/DocumentL10n.h"
#include "mozilla/dom/DocumentL10nBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/L10nUtilsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/l10n/DOMOverlays.h"
#include "nsQueryObject.h"
#include "nsISupports.h"
#include "nsImportModule.h"
#include "nsContentUtils.h"

#define INTL_APP_LOCALES_CHANGED "intl:app-locales-changed"

#define L10N_PSEUDO_PREF "intl.l10n.pseudo"

#define INTL_UI_DIRECTION_PREF "intl.uidirection"

static const char* kObservedPrefs[] = {L10N_PSEUDO_PREF, INTL_UI_DIRECTION_PREF,
                                       nullptr};

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(PromiseResolver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(PromiseResolver)
NS_IMPL_RELEASE(PromiseResolver)

PromiseResolver::PromiseResolver(Promise* aPromise) { mPromise = aPromise; }

void PromiseResolver::ResolvedCallback(JSContext* aCx,
                                       JS::Handle<JS::Value> aValue) {
  mPromise->MaybeResolveWithClone(aCx, aValue);
}

void PromiseResolver::RejectedCallback(JSContext* aCx,
                                       JS::Handle<JS::Value> aValue) {
  mPromise->MaybeRejectWithClone(aCx, aValue);
}

PromiseResolver::~PromiseResolver() { mPromise = nullptr; }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DocumentL10n, mDocument, mDOMLocalization,
                                      mContentSink, mReady)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DocumentL10n)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DocumentL10n)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DocumentL10n)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

DocumentL10n::DocumentL10n(Document* aDocument)
    : mDocument(aDocument), mState(DocumentL10nState::Initialized) {
  mContentSink = do_QueryInterface(aDocument->GetCurrentContentSink());
}

DocumentL10n::~DocumentL10n() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, INTL_APP_LOCALES_CHANGED);
  }

  Preferences::RemoveObservers(this, kObservedPrefs);
}

bool DocumentL10n::Init(nsTArray<nsString>& aResourceIds) {
  nsCOMPtr<mozIDOMLocalizationJSM> jsm =
      do_ImportModule("resource://gre/modules/DOMLocalization.jsm");
  MOZ_RELEASE_ASSERT(jsm);

  Unused << jsm->GetDOMLocalization(getter_AddRefs(mDOMLocalization));
  MOZ_RELEASE_ASSERT(mDOMLocalization);

  nsIGlobalObject* global = mDocument->GetScopeObject();
  if (!global) {
    return false;
  }

  ErrorResult rv;
  mReady = Promise::Create(global, rv);
  if (rv.Failed()) {
    return false;
  }

  // The `aEager = true` here allows us to eagerly trigger
  // resource fetching to increase the chance that the l10n
  // resources will be ready by the time the document
  // is ready for localization.
  uint32_t ret;
  if (NS_FAILED(mDOMLocalization->AddResourceIds(aResourceIds, true, &ret))) {
    return false;
  }

  // Register observers for this instance of
  // DOMLocalization to allow it to retranslate
  // the document when locale changes or pseudolocalization
  // gets turned on.
  RegisterObservers();

  return true;
}

void DocumentL10n::RegisterObservers() {
  DebugOnly<nsresult> rv = Preferences::AddWeakObservers(this, kObservedPrefs);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, INTL_APP_LOCALES_CHANGED, true);
  }
}

NS_IMETHODIMP
DocumentL10n::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  if (!strcmp(aTopic, INTL_APP_LOCALES_CHANGED)) {
    if (mDOMLocalization) {
      mDOMLocalization->OnChange();
    }
  } else {
    MOZ_ASSERT(!strcmp("nsPref:changed", aTopic));
    nsDependentString pref(aData);
    if (pref.EqualsLiteral(L10N_PSEUDO_PREF) ||
        pref.EqualsLiteral(INTL_UI_DIRECTION_PREF)) {
      if (mDOMLocalization) {
        mDOMLocalization->OnChange();
      }
    }
  }

  return NS_OK;
}

void DocumentL10n::Destroy() {
  if (mDOMLocalization) {
    Element* elem = mDocument->GetDocumentElement();
    if (elem) {
      mDOMLocalization->DisconnectRoot(elem);
    }
  }
}

JSObject* DocumentL10n::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return DocumentL10n_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> DocumentL10n::MaybeWrapPromise(
    Promise* aInnerPromise) {
  // For system principal we don't need to wrap the
  // result promise at all.
  if (nsContentUtils::IsSystemPrincipal(mDocument->NodePrincipal())) {
    return RefPtr<Promise>(aInnerPromise).forget();
  }

  nsIGlobalObject* global = mDocument->GetScopeObject();
  if (!global) {
    return nullptr;
  }

  ErrorResult result;
  RefPtr<Promise> docPromise = Promise::Create(global, result);
  if (result.Failed()) {
    return nullptr;
  }

  RefPtr<PromiseResolver> resolver = new PromiseResolver(docPromise);
  aInnerPromise->AppendNativeHandler(resolver);
  return docPromise.forget();
}

uint32_t DocumentL10n::AddResourceIds(nsTArray<nsString>& aResourceIds) {
  uint32_t ret = 0;
  mDOMLocalization->AddResourceIds(aResourceIds, false, &ret);
  return ret;
}

uint32_t DocumentL10n::RemoveResourceIds(nsTArray<nsString>& aResourceIds) {
  // We need to guard against a scenario where the
  // mDOMLocalization has been unlinked, but the elements
  // are only now removed from DOM.
  if (!mDOMLocalization) {
    return 0;
  }
  uint32_t ret = 0;
  mDOMLocalization->RemoveResourceIds(aResourceIds, &ret);
  return ret;
}

already_AddRefed<Promise> DocumentL10n::FormatMessages(
    JSContext* aCx, const Sequence<L10nKey>& aKeys, ErrorResult& aRv) {
  nsTArray<JS::Value> jsKeys;
  SequenceRooter<JS::Value> rooter(aCx, &jsKeys);
  for (auto& key : aKeys) {
    JS::RootedValue jsKey(aCx);
    if (!ToJSValue(aCx, key, &jsKey)) {
      aRv.NoteJSContextException(aCx);
      return nullptr;
    }
    jsKeys.AppendElement(jsKey);
  }

  RefPtr<Promise> promise;
  aRv = mDOMLocalization->FormatMessages(jsKeys, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> DocumentL10n::FormatValues(
    JSContext* aCx, const Sequence<L10nKey>& aKeys, ErrorResult& aRv) {
  nsTArray<JS::Value> jsKeys;
  SequenceRooter<JS::Value> rooter(aCx, &jsKeys);
  for (auto& key : aKeys) {
    JS::RootedValue jsKey(aCx);
    if (!ToJSValue(aCx, key, &jsKey)) {
      aRv.NoteJSContextException(aCx);
      return nullptr;
    }
    jsKeys.AppendElement(jsKey);
  }

  RefPtr<Promise> promise;
  aRv = mDOMLocalization->FormatValues(jsKeys, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> DocumentL10n::FormatValue(
    JSContext* aCx, const nsAString& aId,
    const Optional<JS::Handle<JSObject*>>& aArgs, ErrorResult& aRv) {
  JS::Rooted<JS::Value> args(aCx);

  if (aArgs.WasPassed()) {
    args = JS::ObjectValue(*aArgs.Value());
  } else {
    args = JS::UndefinedValue();
  }

  RefPtr<Promise> promise;
  nsresult rv =
      mDOMLocalization->FormatValue(aId, args, getter_AddRefs(promise));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }
  return MaybeWrapPromise(promise);
}

void DocumentL10n::SetAttributes(JSContext* aCx, Element& aElement,
                                 const nsAString& aId,
                                 const Optional<JS::Handle<JSObject*>>& aArgs,
                                 ErrorResult& aRv) {
  if (!aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::datal10nid, aId,
                            eCaseMatters)) {
    aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::datal10nid, aId, true);
  }

  if (aArgs.WasPassed()) {
    nsAutoString data;
    JS::Rooted<JS::Value> val(aCx, JS::ObjectValue(*aArgs.Value()));
    if (!nsContentUtils::StringifyJSON(aCx, &val, data)) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }
    if (!aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::datal10nargs, data,
                              eCaseMatters)) {
      aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::datal10nargs, data, true);
    }
  } else {
    aElement.UnsetAttr(kNameSpaceID_None, nsGkAtoms::datal10nargs, true);
  }
}

void DocumentL10n::GetAttributes(JSContext* aCx, Element& aElement,
                                 L10nKey& aResult, ErrorResult& aRv) {
  nsAutoString l10nId;
  nsAutoString l10nArgs;

  aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nid, l10nId);
  aResult.mId = l10nId;

  aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nargs, l10nArgs);
  if (!l10nArgs.IsEmpty()) {
    JS::Rooted<JS::Value> json(aCx);
    if (!JS_ParseJSON(aCx, l10nArgs.get(), l10nArgs.Length(), &json)) {
      aRv.NoteJSContextException(aCx);
      return;
    } else if (!json.isObject()) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }
    aResult.mArgs.Construct();
    aResult.mArgs.Value() = &json.toObject();
  }
}

class LocalizationHandler : public PromiseNativeHandler {
 public:
  explicit LocalizationHandler(nsINode* aNode) { mNode = aNode; };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(LocalizationHandler)

  nsTArray<nsCOMPtr<Element>>& Elements() { return mElements; }

  void SetReturnValuePromise(Promise* aReturnValuePromise) {
    mReturnValuePromise = aReturnValuePromise;
  }

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    ErrorResult rv;

    RefPtr<DocumentL10n> docL10n = mNode->OwnerDoc()->GetL10n();

    nsTArray<L10nValue> l10nData;
    if (aValue.isObject()) {
      JS::ForOfIterator iter(aCx);
      if (!iter.init(aValue, JS::ForOfIterator::AllowNonIterable)) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }
      if (!iter.valueIsIterable()) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }

      JS::Rooted<JS::Value> temp(aCx);
      while (true) {
        bool done;
        if (!iter.next(&temp, &done)) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }

        if (done) {
          break;
        }

        L10nValue* slotPtr = l10nData.AppendElement(mozilla::fallible);
        if (!slotPtr) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }

        if (!slotPtr->Init(aCx, temp)) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }
      }
    }

    if (mElements.Length() != l10nData.Length()) {
      mReturnValuePromise->MaybeRejectWithUndefined();
      return;
    }

    if (docL10n) {
      docL10n->PauseObserving(rv);
      if (NS_WARN_IF(rv.Failed())) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }
    }

    nsTArray<DOMOverlaysError> errors;
    for (size_t i = 0; i < l10nData.Length(); ++i) {
      Element* elem = mElements[i];
      mozilla::dom::l10n::DOMOverlays::TranslateElement(*elem, l10nData[i],
                                                        errors, rv);
      if (NS_WARN_IF(rv.Failed())) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }
    }

    if (docL10n) {
      docL10n->ResumeObserving(rv);
      if (NS_WARN_IF(rv.Failed())) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }
    }

    nsTArray<JS::Value> jsErrors;
    SequenceRooter<JS::Value> rooter(aCx, &jsErrors);
    for (auto& error : errors) {
      JS::RootedValue jsError(aCx);
      if (!ToJSValue(aCx, error, &jsError)) {
        mReturnValuePromise->MaybeRejectWithUndefined();
        return;
      }
      jsErrors.AppendElement(jsError);
    }

    mReturnValuePromise->MaybeResolve(jsErrors);
  }

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    mReturnValuePromise->MaybeRejectWithClone(aCx, aValue);
  }

 private:
  ~LocalizationHandler() = default;

  nsTArray<nsCOMPtr<Element>> mElements;
  RefPtr<nsINode> mNode;
  RefPtr<Promise> mReturnValuePromise;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LocalizationHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(LocalizationHandler)

NS_IMPL_CYCLE_COLLECTING_ADDREF(LocalizationHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LocalizationHandler)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(LocalizationHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReturnValuePromise)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(LocalizationHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReturnValuePromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

already_AddRefed<Promise> DocumentL10n::TranslateFragment(JSContext* aCx,
                                                          nsINode& aNode,
                                                          ErrorResult& aRv) {
  Sequence<L10nKey> l10nKeys;
  SequenceRooter<L10nKey> rooter(aCx, &l10nKeys);
  RefPtr<LocalizationHandler> nativeHandler = new LocalizationHandler(&aNode);
  nsTArray<nsCOMPtr<Element>>& domElements = nativeHandler->Elements();
  nsIContent* node =
      aNode.IsContent() ? aNode.AsContent() : aNode.GetFirstChild();
  for (; node; node = node->GetNextNode(&aNode)) {
    if (!node->IsElement()) {
      continue;
    }

    Element* domElement = node->AsElement();

    if (!domElement->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
      continue;
    }

    L10nKey* key = l10nKeys.AppendElement(fallible);
    if (!key) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    GetAttributes(aCx, *domElement, *key, aRv);
    if (aRv.Failed()) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    if (!domElements.AppendElement(domElement, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  nsIGlobalObject* global = mDocument->GetScopeObject();
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Promise> callbackResult = FormatMessages(aCx, l10nKeys, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nativeHandler->SetReturnValuePromise(promise);
  callbackResult->AppendNativeHandler(nativeHandler);

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> DocumentL10n::TranslateElements(
    const Sequence<OwningNonNull<Element>>& aElements, ErrorResult& aRv) {
  AutoTArray<RefPtr<Element>, 10> elements;
  elements.SetCapacity(aElements.Length());
  for (auto& element : aElements) {
    elements.AppendElement(element);
  }
  RefPtr<Promise> promise;
  aRv = mDOMLocalization->TranslateElements(elements, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }
  return MaybeWrapPromise(promise);
}

void DocumentL10n::PauseObserving(ErrorResult& aRv) {
  aRv = mDOMLocalization->PauseObserving();
}

void DocumentL10n::ResumeObserving(ErrorResult& aRv) {
  aRv = mDOMLocalization->ResumeObserving();
}

class L10nReadyHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(L10nReadyHandler)

  explicit L10nReadyHandler(Promise* aPromise, DocumentL10n* aDocumentL10n)
      : mPromise(aPromise), mDocumentL10n(aDocumentL10n) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    mDocumentL10n->InitialDocumentTranslationCompleted();
    mPromise->MaybeResolveWithClone(aCx, aValue);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    mDocumentL10n->InitialDocumentTranslationCompleted();
    mPromise->MaybeRejectWithClone(aCx, aValue);
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

void DocumentL10n::TriggerInitialDocumentTranslation() {
  if (mState >= DocumentL10nState::InitialTranslationTriggered) {
    return;
  }

  mState = DocumentL10nState::InitialTranslationTriggered;

  Element* elem = mDocument->GetDocumentElement();
  if (elem) {
    mDOMLocalization->ConnectRoot(elem);
  }

  RefPtr<Promise> promise;
  mDOMLocalization->TranslateRoots(getter_AddRefs(promise));
  if (!promise) {
    return;
  }

  RefPtr<PromiseNativeHandler> l10nReadyHandler =
      new L10nReadyHandler(mReady, this);
  promise->AppendNativeHandler(l10nReadyHandler);
}

void DocumentL10n::InitialDocumentTranslationCompleted() {
  if (mState >= DocumentL10nState::InitialTranslationCompleted) {
    return;
  }

  mState = DocumentL10nState::InitialTranslationCompleted;

  mDocument->InitialDocumentTranslationCompleted();

  // In XUL scenario mContentSink is nullptr.
  if (mContentSink) {
    mContentSink->InitialDocumentTranslationCompleted();
    mContentSink = nullptr;
  }
}

Promise* DocumentL10n::Ready() { return mReady; }

}  // namespace dom
}  // namespace mozilla
