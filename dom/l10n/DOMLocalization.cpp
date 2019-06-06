#include "js/ForOfIterator.h"  // JS::ForOfIterator
#include "js/JSON.h"           // JS_ParseJSON
#include "nsImportModule.h"
#include "nsIScriptError.h"
#include "DOMLocalization.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/l10n/DOMOverlays.h"

#define INTL_APP_LOCALES_CHANGED "intl:app-locales-changed"

#define L10N_PSEUDO_PREF "intl.l10n.pseudo"

#define INTL_UI_DIRECTION_PREF "intl.uidirection"

static const char* kObservedPrefs[] = {L10N_PSEUDO_PREF, INTL_UI_DIRECTION_PREF,
                                       nullptr};

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::l10n;
using namespace mozilla::intl;

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMLocalization)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMLocalization)
  tmp->DisconnectMutations();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMutations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalization)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoots)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMLocalization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMutations)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoots)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(DOMLocalization)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMLocalization)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMLocalization)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMLocalization)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

DOMLocalization::DOMLocalization(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  mMutations = new mozilla::dom::l10n::Mutations(this);
}

already_AddRefed<DOMLocalization> DOMLocalization::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMLocalization> loc = new DOMLocalization(global);
  return loc.forget();
}

void DOMLocalization::Init(nsTArray<nsString>& aResourceIds, ErrorResult& aRv) {
  nsCOMPtr<mozILocalizationJSM> jsm =
      do_ImportModule("resource://gre/modules/Localization.jsm");
  MOZ_RELEASE_ASSERT(jsm);

  Unused << jsm->GetLocalization(getter_AddRefs(mLocalization));
  MOZ_RELEASE_ASSERT(mLocalization);

  // The `aEager = true` here allows us to eagerly trigger
  // resource fetching to increase the chance that the l10n
  // resources will be ready by the time the document
  // is ready for localization.
  uint32_t ret;
  if (NS_FAILED(mLocalization->AddResourceIds(aResourceIds, true, &ret))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // Register observers for this instance of
  // DOMLocalization to allow it to retranslate
  // the document when locale changes or pseudolocalization
  // gets turned on.
  RegisterObservers();
}

nsIGlobalObject* DOMLocalization::GetParentObject() const { return mGlobal; }

JSObject* DOMLocalization::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return DOMLocalization_Binding::Wrap(aCx, this, aGivenProto);
}

DOMLocalization::~DOMLocalization() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, INTL_APP_LOCALES_CHANGED);
  }

  Preferences::RemoveObservers(this, kObservedPrefs);

  DisconnectMutations();
}

/**
 * Localization API
 */

uint32_t DOMLocalization::AddResourceIds(const nsTArray<nsString>& aResourceIds,
                                         bool aEager) {
  uint32_t ret = 0;
  mLocalization->AddResourceIds(aResourceIds, aEager, &ret);
  return ret;
}

uint32_t DOMLocalization::RemoveResourceIds(
    const nsTArray<nsString>& aResourceIds) {
  // We need to guard against a scenario where the
  // mLocalization has been unlinked, but the elements
  // are only now removed from DOM.
  if (!mLocalization) {
    return 0;
  }
  uint32_t ret = 0;
  mLocalization->RemoveResourceIds(aResourceIds, &ret);
  return ret;
}

already_AddRefed<Promise> DOMLocalization::FormatValue(
    JSContext* aCx, const nsAString& aId,
    const Optional<JS::Handle<JSObject*>>& aArgs, ErrorResult& aRv) {
  JS::Rooted<JS::Value> args(aCx);

  if (aArgs.WasPassed()) {
    args = JS::ObjectValue(*aArgs.Value());
  } else {
    args = JS::UndefinedValue();
  }

  RefPtr<Promise> promise;
  nsresult rv = mLocalization->FormatValue(aId, args, getter_AddRefs(promise));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }
  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> DOMLocalization::FormatValues(
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
  aRv = mLocalization->FormatValues(jsKeys, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  return MaybeWrapPromise(promise);
}

already_AddRefed<Promise> DOMLocalization::FormatMessages(
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
  aRv = mLocalization->FormatMessages(jsKeys, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  return MaybeWrapPromise(promise);
}

/**
 * DOMLocalization API
 */

void DOMLocalization::ConnectRoot(Element& aNode, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = aNode.GetOwnerGlobal();
  if (!global) {
    return;
  }
  MOZ_ASSERT(global == mGlobal,
             "Cannot add a root that overlaps with existing root.");

#ifdef DEBUG
  for (auto iter = mRoots.ConstIter(); !iter.Done(); iter.Next()) {
    Element* root = iter.Get()->GetKey();

    MOZ_ASSERT(
        root != &aNode && !root->Contains(&aNode) && !aNode.Contains(root),
        "Cannot add a root that overlaps with existing root.");
  }
#endif

  mRoots.PutEntry(&aNode);

  aNode.AddMutationObserverUnlessExists(mMutations);
}

void DOMLocalization::DisconnectRoot(Element& aNode, ErrorResult& aRv) {
  if (mRoots.Contains(&aNode)) {
    aNode.RemoveMutationObserver(mMutations);
    mRoots.RemoveEntry(&aNode);
  }
}

void DOMLocalization::PauseObserving(ErrorResult& aRv) {
  mMutations->PauseObserving();
}

void DOMLocalization::ResumeObserving(ErrorResult& aRv) {
  mMutations->ResumeObserving();
}

void DOMLocalization::SetAttributes(
    JSContext* aCx, Element& aElement, const nsAString& aId,
    const Optional<JS::Handle<JSObject*>>& aArgs, ErrorResult& aRv) {
  if (!aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::datal10nid, aId,
                            eCaseMatters)) {
    aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::datal10nid, aId, true);
  }

  if (aArgs.WasPassed() && aArgs.Value()) {
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

void DOMLocalization::GetAttributes(JSContext* aCx, Element& aElement,
                                    L10nKey& aResult, ErrorResult& aRv) {
  nsAutoString l10nId;
  nsAutoString l10nArgs;

  if (aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nid, l10nId)) {
    aResult.mId = l10nId;
  }

  if (aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nargs, l10nArgs)) {
    JS::Rooted<JS::Value> json(aCx);
    if (!JS_ParseJSON(aCx, l10nArgs.get(), l10nArgs.Length(), &json)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
    if (!json.isObject()) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }
    aResult.mArgs = &json.toObject();
  }
}

already_AddRefed<Promise> DOMLocalization::TranslateFragment(nsINode& aNode,
                                                             ErrorResult& aRv) {
  Sequence<OwningNonNull<Element>> elements;

  GetTranslatables(aNode, elements, aRv);

  return TranslateElements(elements, aRv);
}

/**
 * A Promise Handler used to apply the result of
 * a call to Localization::FormatMessages onto the list
 * of translatable elements.
 */
class ElementTranslationHandler : public PromiseNativeHandler {
 public:
  explicit ElementTranslationHandler(DOMLocalization* aDOMLocalization) {
    mDOMLocalization = aDOMLocalization;
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ElementTranslationHandler)

  nsTArray<nsCOMPtr<Element>>& Elements() { return mElements; }

  void SetReturnValuePromise(Promise* aReturnValuePromise) {
    mReturnValuePromise = aReturnValuePromise;
  }

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    ErrorResult rv;

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

    mDOMLocalization->ApplyTranslations(mElements, l10nData, rv);
    if (NS_WARN_IF(rv.Failed())) {
      mReturnValuePromise->MaybeRejectWithUndefined();
      return;
    }

    mReturnValuePromise->MaybeResolveWithUndefined();
  }

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    mReturnValuePromise->MaybeRejectWithClone(aCx, aValue);
  }

 private:
  ~ElementTranslationHandler() = default;

  nsTArray<nsCOMPtr<Element>> mElements;
  RefPtr<DOMLocalization> mDOMLocalization;
  RefPtr<Promise> mReturnValuePromise;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ElementTranslationHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(ElementTranslationHandler)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ElementTranslationHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ElementTranslationHandler)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ElementTranslationHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMLocalization)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReturnValuePromise)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ElementTranslationHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMLocalization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReturnValuePromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

already_AddRefed<Promise> DOMLocalization::TranslateElements(
    const Sequence<OwningNonNull<Element>>& aElements, ErrorResult& aRv) {
  JS::RootingContext* rcx = RootingCx();
  Sequence<L10nKey> l10nKeys;
  SequenceRooter<L10nKey> rooter(rcx, &l10nKeys);
  RefPtr<ElementTranslationHandler> nativeHandler =
      new ElementTranslationHandler(this);
  nsTArray<nsCOMPtr<Element>>& domElements = nativeHandler->Elements();
  domElements.SetCapacity(aElements.Length());

  if (!mGlobal) {
    return nullptr;
  }

  AutoEntryScript aes(mGlobal, "DOMLocalization GetAttributes");
  JSContext* cx = aes.cx();

  for (auto& domElement : aElements) {
    if (!domElement->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
      continue;
    }

    L10nKey* key = l10nKeys.AppendElement(fallible);
    if (!key) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    GetAttributes(cx, *domElement, *key, aRv);
    if (aRv.Failed()) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    if (!domElements.AppendElement(domElement, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Promise> callbackResult = FormatMessages(cx, l10nKeys, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nativeHandler->SetReturnValuePromise(promise);
  callbackResult->AppendNativeHandler(nativeHandler);

  return MaybeWrapPromise(promise);
}

/**
 * Promise handler used to set localization data on
 * roots of elements that got successfully translated.
 */
class L10nRootTranslationHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(L10nRootTranslationHandler)

  explicit L10nRootTranslationHandler(Element* aRoot) : mRoot(aRoot) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    DOMLocalization::SetRootInfo(mRoot);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
  }

 private:
  ~L10nRootTranslationHandler() = default;

  RefPtr<Element> mRoot;
};

NS_IMPL_CYCLE_COLLECTION(L10nRootTranslationHandler, mRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(L10nRootTranslationHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(L10nRootTranslationHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(L10nRootTranslationHandler)

already_AddRefed<Promise> DOMLocalization::TranslateRoots(ErrorResult& aRv) {
  nsTArray<RefPtr<Promise>> promises;

  for (auto iter = mRoots.ConstIter(); !iter.Done(); iter.Next()) {
    Element* root = iter.Get()->GetKey();

    RefPtr<Promise> promise = TranslateFragment(*root, aRv);
    RefPtr<L10nRootTranslationHandler> nativeHandler =
        new L10nRootTranslationHandler(root);
    promise->AppendNativeHandler(nativeHandler);
    promises.AppendElement(promise);
  }
  AutoEntryScript aes(mGlobal, "DOMLocalization TranslateRoots");
  return Promise::All(aes.cx(), promises, aRv);
}

/**
 * Helper methods
 */

/* static */
void DOMLocalization::GetTranslatables(
    nsINode& aNode, Sequence<OwningNonNull<Element>>& aElements,
    ErrorResult& aRv) {
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

    if (!aElements.AppendElement(*domElement, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }
}

/* static */
void DOMLocalization::SetRootInfo(Element* aElement) {
  nsAutoCString primaryLocale;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(primaryLocale);
  aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::lang,
                    NS_ConvertUTF8toUTF16(primaryLocale), true);

  nsAutoString dir;
  if (LocaleService::GetInstance()->IsAppLocaleRTL()) {
    nsGkAtoms::rtl->ToString(dir);
  } else {
    nsGkAtoms::ltr->ToString(dir);
  }

  uint32_t nameSpace = aElement->GetNameSpaceID();
  nsAtom* dirAtom =
      nameSpace == kNameSpaceID_XUL ? nsGkAtoms::localedir : nsGkAtoms::dir;

  aElement->SetAttr(kNameSpaceID_None, dirAtom, dir, true);
}

void DOMLocalization::ApplyTranslations(nsTArray<nsCOMPtr<Element>>& aElements,
                                        nsTArray<L10nValue>& aTranslations,
                                        ErrorResult& aRv) {
  if (aElements.Length() != aTranslations.Length()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  PauseObserving(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsTArray<DOMOverlaysError> errors;
  for (size_t i = 0; i < aTranslations.Length(); ++i) {
    Element* elem = aElements[i];
    mozilla::dom::l10n::DOMOverlays::TranslateElement(*elem, aTranslations[i],
                                                      errors, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  ResumeObserving(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  ReportDOMOverlaysErrors(errors);
}

/* Protected */

void DOMLocalization::RegisterObservers() {
  DebugOnly<nsresult> rv = Preferences::AddWeakObservers(this, kObservedPrefs);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, INTL_APP_LOCALES_CHANGED, true);
  }
}

NS_IMETHODIMP
DOMLocalization::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  if (!strcmp(aTopic, INTL_APP_LOCALES_CHANGED)) {
    OnChange();
  } else {
    MOZ_ASSERT(!strcmp("nsPref:changed", aTopic));
    nsDependentString pref(aData);
    if (pref.EqualsLiteral(L10N_PSEUDO_PREF) ||
        pref.EqualsLiteral(INTL_UI_DIRECTION_PREF)) {
      OnChange();
    }
  }

  return NS_OK;
}

void DOMLocalization::OnChange() {
  if (mLocalization) {
    ErrorResult rv;
    mLocalization->OnChange();
    RefPtr<Promise> promise = TranslateRoots(rv);
  }
}

void DOMLocalization::DisconnectMutations() {
  if (mMutations) {
    mMutations->Disconnect();
    DisconnectRoots();
  }
}

void DOMLocalization::DisconnectRoots() {
  for (auto iter = mRoots.ConstIter(); !iter.Done(); iter.Next()) {
    Element* elem = iter.Get()->GetKey();

    elem->RemoveMutationObserver(mMutations);
  }
  mRoots.Clear();
}

/**
 * PromiseResolver is a PromiseNativeHandler used
 * by MaybeWrapPromise method.
 */
class PromiseResolver final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  explicit PromiseResolver(Promise* aPromise);
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

 protected:
  virtual ~PromiseResolver();

  RefPtr<Promise> mPromise;
};

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

/**
 * MaybeWrapPromise is a helper method used by Localization
 * API methods to clone the value returned by a promise
 * into a new context.
 *
 * This allows for a promise from a privileged context
 * to be returned into an unprivileged document.
 *
 * This method is only used for promises that carry values.
 */
already_AddRefed<Promise> DOMLocalization::MaybeWrapPromise(
    Promise* aInnerPromise) {
  // For system principal we don't need to wrap the
  // result promise at all.
  nsIPrincipal* principal = mGlobal->PrincipalOrNull();
  if (principal && nsContentUtils::IsSystemPrincipal(principal)) {
    return RefPtr<Promise>(aInnerPromise).forget();
  }

  ErrorResult result;
  RefPtr<Promise> docPromise = Promise::Create(mGlobal, result);
  if (result.Failed()) {
    return nullptr;
  }

  RefPtr<PromiseResolver> resolver = new PromiseResolver(docPromise);
  aInnerPromise->AppendNativeHandler(resolver);
  return docPromise.forget();
}

void DOMLocalization::ReportDOMOverlaysErrors(
    nsTArray<mozilla::dom::DOMOverlaysError>& aErrors) {
  nsAutoString msg;

  for (auto& error : aErrors) {
    if (error.mCode.WasPassed()) {
      msg = NS_LITERAL_STRING("[fluent-dom] ");
      switch (error.mCode.Value()) {
        case DOMOverlays_Binding::ERROR_FORBIDDEN_TYPE:
          msg += NS_LITERAL_STRING("An element of forbidden type \"") +
                 error.mTranslatedElementName.Value() +
                 NS_LITERAL_STRING(
                     "\" was found in the translation. Only safe text-level "
                     "elements and elements with data-l10n-name are allowed.");
          break;
        case DOMOverlays_Binding::ERROR_NAMED_ELEMENT_MISSING:
          msg += NS_LITERAL_STRING("An element named \"") +
                 error.mL10nName.Value() +
                 NS_LITERAL_STRING("\" wasn't found in the source.");
          break;
        case DOMOverlays_Binding::ERROR_NAMED_ELEMENT_TYPE_MISMATCH:
          msg += NS_LITERAL_STRING("An element named \"") +
                 error.mL10nName.Value() +
                 NS_LITERAL_STRING(
                     "\" was found in the translation but its type ") +
                 error.mTranslatedElementName.Value() +
                 NS_LITERAL_STRING(
                     " didn't match the element found in the source ") +
                 error.mSourceElementName.Value() + NS_LITERAL_STRING(".");
          break;
        case DOMOverlays_Binding::ERROR_UNKNOWN:
        default:
          msg += NS_LITERAL_STRING(
              "Unknown error happened while translation of an element.");
          break;
      }
      nsPIDOMWindowInner* innerWindow = GetParentObject()->AsInnerWindow();
      Document* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;
      if (doc) {
        nsContentUtils::ReportToConsoleNonLocalized(
            msg, nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM"), doc);
      } else {
        NS_WARNING("Failed to report l10n DOM Overlay errors to console.");
      }
    }
  }
}
