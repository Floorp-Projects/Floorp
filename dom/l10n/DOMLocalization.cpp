/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ForOfIterator.h"  // JS::ForOfIterator
#include "js/JSON.h"           // JS_ParseJSON
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "DOMLocalization.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/L10nOverlays.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::intl;

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMLocalization)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMLocalization, Localization)
  tmp->DisconnectMutations();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMutations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoots)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMLocalization, Localization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMutations)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoots)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DOMLocalization, Localization)
NS_IMPL_RELEASE_INHERITED(DOMLocalization, Localization)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMLocalization)
NS_INTERFACE_MAP_END_INHERITING(Localization)

DOMLocalization::DOMLocalization(nsIGlobalObject* aGlobal, bool aSync)
    : Localization(aGlobal, aSync) {
  mMutations = new L10nMutations(this);
}

DOMLocalization::DOMLocalization(nsIGlobalObject* aGlobal, bool aIsSync,
                                 const ffi::LocalizationRc* aRaw)
    : Localization(aGlobal, aIsSync, aRaw) {
  mMutations = new L10nMutations(this);
}

already_AddRefed<DOMLocalization> DOMLocalization::Constructor(
    const GlobalObject& aGlobal, const Sequence<nsCString>& aResourceIds,
    bool aIsSync, const Optional<NonNull<L10nRegistry>>& aRegistry,
    const Optional<Sequence<nsCString>>& aLocales, ErrorResult& aRv) {
  nsTArray<nsCString> resIds = ToTArray<nsTArray<nsCString>>(aResourceIds);
  Maybe<nsTArray<nsCString>> locales;

  if (aLocales.WasPassed()) {
    locales.emplace();
    locales->SetCapacity(aLocales.Value().Length());
    for (const auto& locale : aLocales.Value()) {
      locales->AppendElement(locale);
    }
  }

  RefPtr<const ffi::LocalizationRc> raw;
  bool result;

  if (aRegistry.WasPassed()) {
    result = ffi::localization_new_with_locales(
        &resIds, aIsSync, aRegistry.Value().Raw(), locales.ptrOr(nullptr),
        getter_AddRefs(raw));
  } else {
    result = ffi::localization_new_with_locales(
        &resIds, aIsSync, nullptr, locales.ptrOr(nullptr), getter_AddRefs(raw));
  }

  if (result) {
    nsCOMPtr<nsIGlobalObject> global =
        do_QueryInterface(aGlobal.GetAsSupports());

    return do_AddRef(new DOMLocalization(global, aIsSync, raw));
  }
  aRv.ThrowInvalidStateError(
      "Failed to create the Localization. Check the locales arguments.");
  return nullptr;
}

JSObject* DOMLocalization::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return DOMLocalization_Binding::Wrap(aCx, this, aGivenProto);
}

void DOMLocalization::Destroy() { DisconnectMutations(); }

DOMLocalization::~DOMLocalization() { Destroy(); }

/**
 * DOMLocalization API
 */

void DOMLocalization::ConnectRoot(nsINode& aNode, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = aNode.GetOwnerGlobal();
  if (!global) {
    return;
  }
  MOZ_ASSERT(global == mGlobal,
             "Cannot add a root that overlaps with existing root.");

#ifdef DEBUG
  for (nsINode* root : mRoots) {
    MOZ_ASSERT(
        root != &aNode && !root->Contains(&aNode) && !aNode.Contains(root),
        "Cannot add a root that overlaps with existing root.");
  }
#endif

  mRoots.Insert(&aNode);

  aNode.AddMutationObserverUnlessExists(mMutations);
}

void DOMLocalization::DisconnectRoot(nsINode& aNode, ErrorResult& aRv) {
  if (mRoots.Contains(&aNode)) {
    aNode.RemoveMutationObserver(mMutations);
    mRoots.Remove(&aNode);
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
      aRv.NoteJSContextException(aCx);
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

void DOMLocalization::GetAttributes(Element& aElement, L10nIdArgs& aResult,
                                    ErrorResult& aRv) {
  nsAutoString l10nId;
  nsAutoString l10nArgs;

  if (aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nid, l10nId)) {
    CopyUTF16toUTF8(l10nId, aResult.mId);
  }

  if (aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nargs, l10nArgs)) {
    ConvertStringToL10nArgs(l10nArgs, aResult.mArgs.SetValue(), aRv);
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
  explicit ElementTranslationHandler(DOMLocalization* aDOMLocalization,
                                     nsXULPrototypeDocument* aProto)
      : mDOMLocalization(aDOMLocalization), mProto(aProto){};

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ElementTranslationHandler)

  nsTArray<nsCOMPtr<Element>>& Elements() { return mElements; }

  void SetReturnValuePromise(Promise* aReturnValuePromise) {
    mReturnValuePromise = aReturnValuePromise;
  }

  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    ErrorResult rv;

    nsTArray<Nullable<L10nMessage>> l10nData;
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

        Nullable<L10nMessage>* slotPtr =
            l10nData.AppendElement(mozilla::fallible);
        if (!slotPtr) {
          mReturnValuePromise->MaybeRejectWithUndefined();
          return;
        }

        if (!temp.isNull()) {
          if (!slotPtr->SetValue().Init(aCx, temp)) {
            mReturnValuePromise->MaybeRejectWithUndefined();
            return;
          }
        }
      }
    }

    bool allTranslated =
        mDOMLocalization->ApplyTranslations(mElements, l10nData, mProto, rv);
    if (NS_WARN_IF(rv.Failed()) || !allTranslated) {
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
  RefPtr<nsXULPrototypeDocument> mProto;
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProto)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ElementTranslationHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMLocalization)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReturnValuePromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProto)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

already_AddRefed<Promise> DOMLocalization::TranslateElements(
    const Sequence<OwningNonNull<Element>>& aElements, ErrorResult& aRv) {
  return TranslateElements(aElements, nullptr, aRv);
}

already_AddRefed<Promise> DOMLocalization::TranslateElements(
    const Sequence<OwningNonNull<Element>>& aElements,
    nsXULPrototypeDocument* aProto, ErrorResult& aRv) {
  JS::RootingContext* rcx = RootingCx();
  Sequence<OwningUTF8StringOrL10nIdArgs> l10nKeys;
  SequenceRooter<OwningUTF8StringOrL10nIdArgs> rooter(rcx, &l10nKeys);
  RefPtr<ElementTranslationHandler> nativeHandler =
      new ElementTranslationHandler(this, aProto);
  nsTArray<nsCOMPtr<Element>>& domElements = nativeHandler->Elements();
  domElements.SetCapacity(aElements.Length());

  if (!mGlobal) {
    return nullptr;
  }

  for (auto& domElement : aElements) {
    if (!domElement->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
      continue;
    }

    OwningUTF8StringOrL10nIdArgs* key = l10nKeys.AppendElement(fallible);
    if (!key) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    GetAttributes(*domElement, key->SetAsL10nIdArgs(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
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

  if (IsSync()) {
    nsTArray<Nullable<L10nMessage>> l10nMessages;

    FormatMessagesSync(l10nKeys, l10nMessages, aRv);

    bool allTranslated =
        ApplyTranslations(domElements, l10nMessages, aProto, aRv);
    if (NS_WARN_IF(aRv.Failed()) || !allTranslated) {
      promise->MaybeRejectWithUndefined();
      return MaybeWrapPromise(promise);
    }

    promise->MaybeResolveWithUndefined();
  } else {
    RefPtr<Promise> callbackResult = FormatMessages(l10nKeys, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    nativeHandler->SetReturnValuePromise(promise);
    callbackResult->AppendNativeHandler(nativeHandler);
  }

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

  for (nsINode* root : mRoots) {
    RefPtr<Promise> promise = TranslateFragment(*root, aRv);

    // If the root is an element, we'll add a native handler
    // to set root info (language, direction etc.) on it
    // once the localization finishes.
    if (root->IsElement()) {
      RefPtr<L10nRootTranslationHandler> nativeHandler =
          new L10nRootTranslationHandler(root->AsElement());
      promise->AppendNativeHandler(nativeHandler);
    }

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

bool DOMLocalization::ApplyTranslations(
    nsTArray<nsCOMPtr<Element>>& aElements,
    nsTArray<Nullable<L10nMessage>>& aTranslations,
    nsXULPrototypeDocument* aProto, ErrorResult& aRv) {
  if (aElements.Length() != aTranslations.Length()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  PauseObserving(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  bool hasMissingTranslation = false;

  nsTArray<L10nOverlaysError> errors;
  for (size_t i = 0; i < aTranslations.Length(); ++i) {
    nsCOMPtr elem = aElements[i];
    if (aTranslations[i].IsNull()) {
      hasMissingTranslation = true;
      continue;
    }
    // If we have a proto, we expect all elements are connected up.
    // If they're not, they may have been removed by earlier translations.
    // We will have added an error in L10nOverlays in this case.
    // This is an error in fluent use, but shouldn't be crashing. There's
    // also no point translating the element - skip it:
    if (aProto && !elem->IsInComposedDoc()) {
      continue;
    }

    // It is possible that someone removed the `data-l10n-id` from the element
    // before the async translation completed. In that case, skip applying
    // the translation.
    if (!elem->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
      continue;
    }
    L10nOverlays::TranslateElement(*elem, aTranslations[i].Value(), errors,
                                   aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      hasMissingTranslation = true;
      continue;
    }
    if (aProto) {
      // We only need to rebuild deep if the translation has a value.
      // Otherwise we'll only rebuild the attributes.
      aProto->RebuildL10nPrototype(elem,
                                   !aTranslations[i].Value().mValue.IsVoid());
    }
  }

  ReportL10nOverlaysErrors(errors);

  ResumeObserving(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return !hasMissingTranslation;
}

/* Protected */

void DOMLocalization::OnChange() {
  Localization::OnChange();
  RefPtr<Promise> promise = TranslateRoots(IgnoreErrors());
}

void DOMLocalization::DisconnectMutations() {
  if (mMutations) {
    mMutations->Disconnect();
    DisconnectRoots();
  }
}

void DOMLocalization::DisconnectRoots() {
  for (nsINode* node : mRoots) {
    node->RemoveMutationObserver(mMutations);
  }
  mRoots.Clear();
}

void DOMLocalization::ReportL10nOverlaysErrors(
    nsTArray<L10nOverlaysError>& aErrors) {
  nsAutoString msg;

  for (auto& error : aErrors) {
    if (error.mCode.WasPassed()) {
      msg = u"[fluent-dom] "_ns;
      switch (error.mCode.Value()) {
        case L10nOverlays_Binding::ERROR_FORBIDDEN_TYPE:
          msg += u"An element of forbidden type \""_ns +
                 error.mTranslatedElementName.Value() +
                 nsLiteralString(
                     u"\" was found in the translation. Only safe text-level "
                     "elements and elements with data-l10n-name are allowed.");
          break;
        case L10nOverlays_Binding::ERROR_NAMED_ELEMENT_MISSING:
          msg += u"An element named \""_ns + error.mL10nName.Value() +
                 u"\" wasn't found in the source."_ns;
          break;
        case L10nOverlays_Binding::ERROR_NAMED_ELEMENT_TYPE_MISMATCH:
          msg += u"An element named \""_ns + error.mL10nName.Value() +
                 nsLiteralString(
                     u"\" was found in the translation but its type ") +
                 error.mTranslatedElementName.Value() +
                 nsLiteralString(
                     u" didn't match the element found in the source ") +
                 error.mSourceElementName.Value() + u"."_ns;
          break;
        case L10nOverlays_Binding::ERROR_TRANSLATED_ELEMENT_DISCONNECTED:
          msg += u"The element using message \""_ns + error.mL10nName.Value() +
                 nsLiteralString(
                     u"\" was removed from the DOM when translating its \"") +
                 error.mTranslatedElementName.Value() + u"\" parent."_ns;
          break;
        case L10nOverlays_Binding::ERROR_TRANSLATED_ELEMENT_DISALLOWED_DOM:
          msg += nsLiteralString(
                     u"While translating an element with fluent ID \"") +
                 error.mL10nName.Value() + u"\" a child element of type \""_ns +
                 error.mTranslatedElementName.Value() +
                 nsLiteralString(
                     u"\" was removed. Either the fluent message "
                     "does not contain markup, or it does not contain markup "
                     "of this type.");
          break;
        case L10nOverlays_Binding::ERROR_UNKNOWN:
        default:
          msg += nsLiteralString(
              u"Unknown error happened while translating an element.");
          break;
      }
      nsPIDOMWindowInner* innerWindow = GetParentObject()->AsInnerWindow();
      Document* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;
      if (doc) {
        nsContentUtils::ReportToConsoleNonLocalized(
            msg, nsIScriptError::warningFlag, "DOM"_ns, doc);
      } else {
        NS_WARNING("Failed to report l10n DOM Overlay errors to console.");
      }
      printf_stderr("%s\n", NS_ConvertUTF16toUTF8(msg).get());
    }
  }
}

void DOMLocalization::ConvertStringToL10nArgs(const nsString& aInput,
                                              intl::L10nArgs& aRetVal,
                                              ErrorResult& aRv) {
  // This method uses a temporary dictionary to automate
  // converting a JSON string into an IDL Record via a dictionary.
  //
  // Once we get Record::Init(const nsAString& aJSON), we'll switch to
  // that.
  L10nArgsHelperDict helperDict;
  if (!helperDict.Init(u"{\"args\": "_ns + aInput + u"}"_ns)) {
    nsTArray<nsCString> errors{
        "[dom/l10n] Failed to parse l10n-args JSON: "_ns +
            NS_ConvertUTF16toUTF8(aInput),
    };
    MaybeReportErrorsToGecko(errors, aRv, GetParentObject());
    return;
  }
  for (auto& entry : helperDict.mArgs.Entries()) {
    L10nArgs::EntryType* newEntry = aRetVal.Entries().AppendElement(fallible);
    if (!newEntry) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    newEntry->mKey = entry.mKey;
    newEntry->mValue = entry.mValue;
  }
}
