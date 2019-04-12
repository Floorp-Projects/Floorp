/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CustomElementRegistry.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/CustomElementRegistryBinding.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/ShadowIncludingTreeIterator.h"
#include "mozilla/dom/XULElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebComponentsBinding.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsHTMLTags.h"
#include "jsapi.h"
#include "js/ForOfIterator.h"  // JS::ForOfIterator
#include "xpcprivate.h"
#include "nsGlobalWindow.h"

namespace mozilla {
namespace dom {

//-----------------------------------------------------
// CustomElementUpgradeReaction

class CustomElementUpgradeReaction final : public CustomElementReaction {
 public:
  explicit CustomElementUpgradeReaction(CustomElementDefinition* aDefinition)
      : mDefinition(aDefinition) {
    mIsUpgradeReaction = true;
  }

  virtual void Traverse(
      nsCycleCollectionTraversalCallback& aCb) const override {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mDefinition");
    aCb.NoteNativeChild(
        mDefinition, NS_CYCLE_COLLECTION_PARTICIPANT(CustomElementDefinition));
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    // We don't really own mDefinition.
    return aMallocSizeOf(this);
  }

 private:
  MOZ_CAN_RUN_SCRIPT
  virtual void Invoke(Element* aElement, ErrorResult& aRv) override {
    CustomElementRegistry::Upgrade(aElement, mDefinition, aRv);
  }

  const RefPtr<CustomElementDefinition> mDefinition;
};

//-----------------------------------------------------
// CustomElementCallbackReaction

class CustomElementCallbackReaction final : public CustomElementReaction {
 public:
  explicit CustomElementCallbackReaction(
      UniquePtr<CustomElementCallback> aCustomElementCallback)
      : mCustomElementCallback(std::move(aCustomElementCallback)) {}

  virtual void Traverse(
      nsCycleCollectionTraversalCallback& aCb) const override {
    mCustomElementCallback->Traverse(aCb);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t n = aMallocSizeOf(this);

    n += mCustomElementCallback->SizeOfIncludingThis(aMallocSizeOf);

    return n;
  }

 private:
  virtual void Invoke(Element* aElement, ErrorResult& aRv) override {
    mCustomElementCallback->Call();
  }

  UniquePtr<CustomElementCallback> mCustomElementCallback;
};

//-----------------------------------------------------
// CustomElementCallback

size_t LifecycleCallbackArgs::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = name.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += oldValue.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += newValue.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  n += namespaceURI.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return n;
}

void CustomElementCallback::Call() {
  switch (mType) {
    case Document::eConnected:
      static_cast<LifecycleConnectedCallback*>(mCallback.get())
          ->Call(mThisObject);
      break;
    case Document::eDisconnected:
      static_cast<LifecycleDisconnectedCallback*>(mCallback.get())
          ->Call(mThisObject);
      break;
    case Document::eAdopted:
      static_cast<LifecycleAdoptedCallback*>(mCallback.get())
          ->Call(mThisObject, mAdoptedCallbackArgs.mOldDocument,
                 mAdoptedCallbackArgs.mNewDocument);
      break;
    case Document::eAttributeChanged:
      static_cast<LifecycleAttributeChangedCallback*>(mCallback.get())
          ->Call(mThisObject, mArgs.name, mArgs.oldValue, mArgs.newValue,
                 mArgs.namespaceURI);
      break;
    case Document::eGetCustomInterface:
      MOZ_ASSERT_UNREACHABLE("Don't call GetCustomInterface through callback");
      break;
  }
}

void CustomElementCallback::Traverse(
    nsCycleCollectionTraversalCallback& aCb) const {
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mThisObject");
  aCb.NoteXPCOMChild(mThisObject);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mCallback");
  aCb.NoteXPCOMChild(mCallback);
}

size_t CustomElementCallback::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // We don't uniquely own mThisObject.

  // We own mCallback but it doesn't have any special memory reporting we can do
  // for it other than report its own size.
  n += aMallocSizeOf(mCallback);

  n += mArgs.SizeOfExcludingThis(aMallocSizeOf);

  // mAdoptedCallbackArgs doesn't really uniquely own its members.

  return n;
}

CustomElementCallback::CustomElementCallback(
    Element* aThisObject, Document::ElementCallbackType aCallbackType,
    mozilla::dom::CallbackFunction* aCallback)
    : mThisObject(aThisObject), mCallback(aCallback), mType(aCallbackType) {}

//-----------------------------------------------------
// CustomElementData

CustomElementData::CustomElementData(nsAtom* aType)
    : CustomElementData(aType, CustomElementData::State::eUndefined) {}

CustomElementData::CustomElementData(nsAtom* aType, State aState)
    : mState(aState), mType(aType) {}

void CustomElementData::SetCustomElementDefinition(
    CustomElementDefinition* aDefinition) {
  MOZ_ASSERT(mState == State::eCustom);
  MOZ_ASSERT(!mCustomElementDefinition);
  MOZ_ASSERT(aDefinition->mType == mType);

  mCustomElementDefinition = aDefinition;
}

CustomElementDefinition* CustomElementData::GetCustomElementDefinition() {
  MOZ_ASSERT(mCustomElementDefinition ? mState == State::eCustom
                                      : mState != State::eCustom);

  return mCustomElementDefinition;
}

void CustomElementData::Traverse(
    nsCycleCollectionTraversalCallback& aCb) const {
  for (uint32_t i = 0; i < mReactionQueue.Length(); i++) {
    if (mReactionQueue[i]) {
      mReactionQueue[i]->Traverse(aCb);
    }
  }

  if (mCustomElementDefinition) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mCustomElementDefinition");
    aCb.NoteNativeChild(
        mCustomElementDefinition,
        NS_CYCLE_COLLECTION_PARTICIPANT(CustomElementDefinition));
  }
}

void CustomElementData::Unlink() {
  mReactionQueue.Clear();
  mCustomElementDefinition = nullptr;
}

size_t CustomElementData::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  n += mReactionQueue.ShallowSizeOfExcludingThis(aMallocSizeOf);

  for (auto& reaction : mReactionQueue) {
    // "reaction" can be null if we're being called indirectly from
    // InvokeReactions (e.g. due to a reaction causing a memory report to be
    // captured somehow).
    if (reaction) {
      n += reaction->SizeOfIncludingThis(aMallocSizeOf);
    }
  }

  return n;
}

//-----------------------------------------------------
// CustomElementRegistry

namespace {

class MOZ_RAII AutoConstructionStackEntry final {
 public:
  AutoConstructionStackEntry(nsTArray<RefPtr<Element>>& aStack,
                             Element* aElement)
      : mStack(aStack) {
    MOZ_ASSERT(aElement->IsHTMLElement() || aElement->IsXULElement());

    mIndex = mStack.Length();
    mStack.AppendElement(aElement);
  }

  ~AutoConstructionStackEntry() {
    MOZ_ASSERT(mIndex == mStack.Length() - 1,
               "Removed element should be the last element");
    mStack.RemoveElementAt(mIndex);
  }

 private:
  nsTArray<RefPtr<Element>>& mStack;
  uint32_t mIndex;
};

}  // namespace

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_CLASS(CustomElementRegistry)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CustomElementRegistry)
  tmp->mConstructors.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCustomDefinitions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWhenDefinedPromiseMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElementCreationCallbacks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CustomElementRegistry)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCustomDefinitions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWhenDefinedPromiseMap)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElementCreationCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CustomElementRegistry)
  for (auto iter = tmp->mConstructors.iter(); !iter.done(); iter.next()) {
    aCallbacks.Trace(&iter.get().mutableKey(), "mConstructors key", aClosure);
  }
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CustomElementRegistry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CustomElementRegistry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CustomElementRegistry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CustomElementRegistry::CustomElementRegistry(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow), mIsCustomDefinitionRunning(false) {
  MOZ_ASSERT(aWindow);

  mozilla::HoldJSObjects(this);
}

CustomElementRegistry::~CustomElementRegistry() {
  mozilla::DropJSObjects(this);
}

NS_IMETHODIMP
CustomElementRegistry::RunCustomElementCreationCallback::Run() {
  ErrorResult er;
  nsDependentAtomString value(mAtom);
  mCallback->Call(value, er);
  MOZ_ASSERT(NS_SUCCEEDED(er.StealNSResult()),
             "chrome JavaScript error in the callback.");

  RefPtr<CustomElementDefinition> definition =
      mRegistry->mCustomDefinitions.Get(mAtom);
  MOZ_ASSERT(definition, "Callback should define the definition of type.");
  MOZ_ASSERT(!mRegistry->mElementCreationCallbacks.GetWeak(mAtom),
             "Callback should be removed.");

  nsAutoPtr<nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>> elements;
  mRegistry->mElementCreationCallbacksUpgradeCandidatesMap.Remove(mAtom,
                                                                  &elements);
  MOZ_ASSERT(elements, "There should be a list");

  for (auto iter = elements->Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<Element> elem = do_QueryReferent(iter.Get()->GetKey());
    if (!elem) {
      continue;
    }

    CustomElementRegistry::Upgrade(elem, definition, er);
    MOZ_ASSERT(NS_SUCCEEDED(er.StealNSResult()),
               "chrome JavaScript error in custom element construction.");
  }

  return NS_OK;
}

CustomElementDefinition* CustomElementRegistry::LookupCustomElementDefinition(
    nsAtom* aNameAtom, int32_t aNameSpaceID, nsAtom* aTypeAtom) {
  CustomElementDefinition* data = mCustomDefinitions.GetWeak(aTypeAtom);

  if (!data) {
    RefPtr<CustomElementCreationCallback> callback;
    mElementCreationCallbacks.Get(aTypeAtom, getter_AddRefs(callback));
    if (callback) {
      mElementCreationCallbacks.Remove(aTypeAtom);
      mElementCreationCallbacksUpgradeCandidatesMap.LookupOrAdd(aTypeAtom);
      RefPtr<Runnable> runnable =
          new RunCustomElementCreationCallback(this, aTypeAtom, callback);
      nsContentUtils::AddScriptRunner(runnable.forget());
      data = mCustomDefinitions.GetWeak(aTypeAtom);
    }
  }

  if (data && data->mLocalName == aNameAtom &&
      data->mNamespaceID == aNameSpaceID) {
    return data;
  }

  return nullptr;
}

CustomElementDefinition* CustomElementRegistry::LookupCustomElementDefinition(
    JSContext* aCx, JSObject* aConstructor) const {
  // We're looking up things that tested true for JS::IsConstructor,
  // so doing a CheckedUnwrapStatic is fine here.
  JS::Rooted<JSObject*> constructor(aCx, js::CheckedUnwrapStatic(aConstructor));

  const auto& ptr = mConstructors.lookup(constructor);
  if (!ptr) {
    return nullptr;
  }

  CustomElementDefinition* definition =
      mCustomDefinitions.GetWeak(ptr->value());
  MOZ_ASSERT(definition, "Definition must be found in mCustomDefinitions");

  return definition;
}

void CustomElementRegistry::RegisterUnresolvedElement(Element* aElement,
                                                      nsAtom* aTypeName) {
  // We don't have a use-case for a Custom Element inside NAC, and continuing
  // here causes performance issues for NAC + XBL anonymous content.
  if (aElement->IsInNativeAnonymousSubtree()) {
    return;
  }

  mozilla::dom::NodeInfo* info = aElement->NodeInfo();

  // Candidate may be a custom element through extension,
  // in which case the custom element type name will not
  // match the element tag name. e.g. <button is="x-button">.
  RefPtr<nsAtom> typeName = aTypeName;
  if (!typeName) {
    typeName = info->NameAtom();
  }

  if (mCustomDefinitions.GetWeak(typeName)) {
    return;
  }

  nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>* unresolved =
      mCandidatesMap.LookupOrAdd(typeName);
  nsWeakPtr elem = do_GetWeakReference(aElement);
  unresolved->PutEntry(elem);
}

void CustomElementRegistry::UnregisterUnresolvedElement(Element* aElement,
                                                        nsAtom* aTypeName) {
  nsIWeakReference* weak = aElement->GetExistingWeakReference();
  if (!weak) {
    return;
  }

#ifdef DEBUG
  {
    nsWeakPtr weakPtr = do_GetWeakReference(aElement);
    MOZ_ASSERT(
        weak == weakPtr.get(),
        "do_GetWeakReference should reuse the existing nsIWeakReference.");
  }
#endif

  nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>* candidates = nullptr;
  if (mCandidatesMap.Get(aTypeName, &candidates)) {
    MOZ_ASSERT(candidates);
    candidates->RemoveEntry(weak);
  }
}

/* static */
UniquePtr<CustomElementCallback>
CustomElementRegistry::CreateCustomElementCallback(
    Document::ElementCallbackType aType, Element* aCustomElement,
    LifecycleCallbackArgs* aArgs,
    LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
    CustomElementDefinition* aDefinition) {
  MOZ_ASSERT(aDefinition, "CustomElementDefinition should not be null");
  MOZ_ASSERT(aCustomElement->GetCustomElementData(),
             "CustomElementData should exist");

  // Let CALLBACK be the callback associated with the key NAME in CALLBACKS.
  CallbackFunction* func = nullptr;
  switch (aType) {
    case Document::eConnected:
      if (aDefinition->mCallbacks->mConnectedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mConnectedCallback.Value();
      }
      break;

    case Document::eDisconnected:
      if (aDefinition->mCallbacks->mDisconnectedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mDisconnectedCallback.Value();
      }
      break;

    case Document::eAdopted:
      if (aDefinition->mCallbacks->mAdoptedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mAdoptedCallback.Value();
      }
      break;

    case Document::eAttributeChanged:
      if (aDefinition->mCallbacks->mAttributeChangedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mAttributeChangedCallback.Value();
      }
      break;

    case Document::eGetCustomInterface:
      MOZ_ASSERT_UNREACHABLE("Don't call GetCustomInterface through callback");
      break;
  }

  // If there is no such callback, stop.
  if (!func) {
    return nullptr;
  }

  // Add CALLBACK to ELEMENT's callback queue.
  auto callback =
      MakeUnique<CustomElementCallback>(aCustomElement, aType, func);

  if (aArgs) {
    callback->SetArgs(*aArgs);
  }

  if (aAdoptedCallbackArgs) {
    callback->SetAdoptedCallbackArgs(*aAdoptedCallbackArgs);
  }
  return callback;
}

/* static */
void CustomElementRegistry::EnqueueLifecycleCallback(
    Document::ElementCallbackType aType, Element* aCustomElement,
    LifecycleCallbackArgs* aArgs,
    LifecycleAdoptedCallbackArgs* aAdoptedCallbackArgs,
    CustomElementDefinition* aDefinition) {
  CustomElementDefinition* definition = aDefinition;
  if (!definition) {
    definition = aCustomElement->GetCustomElementDefinition();
    if (!definition ||
        definition->mLocalName != aCustomElement->NodeInfo()->NameAtom()) {
      return;
    }

    if (!definition->mCallbacks) {
      // definition has been unlinked.  Don't try to mess with it.
      return;
    }
  }

  auto callback = CreateCustomElementCallback(aType, aCustomElement, aArgs,
                                              aAdoptedCallbackArgs, definition);
  if (!callback) {
    return;
  }

  DocGroup* docGroup = aCustomElement->OwnerDoc()->GetDocGroup();
  if (!docGroup) {
    return;
  }

  if (aType == Document::eAttributeChanged) {
    RefPtr<nsAtom> attrName = NS_Atomize(aArgs->name);
    if (definition->mObservedAttributes.IsEmpty() ||
        !definition->mObservedAttributes.Contains(attrName)) {
      return;
    }
  }

  CustomElementReactionsStack* reactionsStack =
      docGroup->CustomElementReactionsStack();
  reactionsStack->EnqueueCallbackReaction(aCustomElement, std::move(callback));
}

namespace {

class CandidateFinder {
 public:
  CandidateFinder(nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>& aCandidates,
                  Document* aDoc);
  nsTArray<nsCOMPtr<Element>> OrderedCandidates();

 private:
  nsCOMPtr<Document> mDoc;
  nsInterfaceHashtable<nsPtrHashKey<Element>, Element> mCandidates;
};

CandidateFinder::CandidateFinder(
    nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>& aCandidates,
    Document* aDoc)
    : mDoc(aDoc), mCandidates(aCandidates.Count()) {
  MOZ_ASSERT(mDoc);
  for (auto iter = aCandidates.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<Element> elem = do_QueryReferent(iter.Get()->GetKey());
    if (!elem) {
      continue;
    }

    Element* key = elem.get();
    mCandidates.Put(key, elem.forget());
  }
}

nsTArray<nsCOMPtr<Element>> CandidateFinder::OrderedCandidates() {
  if (mCandidates.Count() == 1) {
    // Fast path for one candidate.
    for (auto iter = mCandidates.Iter(); !iter.Done(); iter.Next()) {
      nsTArray<nsCOMPtr<Element>> rval({std::move(iter.Data())});
      iter.Remove();
      return rval;
    }
  }

  nsTArray<nsCOMPtr<Element>> orderedElements(mCandidates.Count());
  for (nsINode* node : ShadowIncludingTreeIterator(*mDoc)) {
    Element* element = Element::FromNode(node);
    if (!element) {
      continue;
    }

    nsCOMPtr<Element> elem;
    if (mCandidates.Remove(element, getter_AddRefs(elem))) {
      orderedElements.AppendElement(std::move(elem));
      if (mCandidates.Count() == 0) {
        break;
      }
    }
  }

  return orderedElements;
}

}  // namespace

void CustomElementRegistry::UpgradeCandidates(
    nsAtom* aKey, CustomElementDefinition* aDefinition, ErrorResult& aRv) {
  DocGroup* docGroup = mWindow->GetDocGroup();
  if (!docGroup) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsAutoPtr<nsTHashtable<nsRefPtrHashKey<nsIWeakReference>>> candidates;
  if (mCandidatesMap.Remove(aKey, &candidates)) {
    MOZ_ASSERT(candidates);
    CustomElementReactionsStack* reactionsStack =
        docGroup->CustomElementReactionsStack();

    CandidateFinder finder(*candidates, mWindow->GetExtantDoc());
    for (auto& elem : finder.OrderedCandidates()) {
      reactionsStack->EnqueueUpgradeReaction(elem, aDefinition);
    }
  }
}

JSObject* CustomElementRegistry::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return CustomElementRegistry_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* CustomElementRegistry::GetParentObject() const { return mWindow; }

DocGroup* CustomElementRegistry::GetDocGroup() const {
  return mWindow ? mWindow->GetDocGroup() : nullptr;
}

int32_t CustomElementRegistry::InferNamespace(
    JSContext* aCx, JS::Handle<JSObject*> constructor) {
  JS::Rooted<JSObject*> XULConstructor(
      aCx, XULElement_Binding::GetConstructorObject(aCx));

  JS::Rooted<JSObject*> proto(aCx, constructor);
  while (proto) {
    if (proto == XULConstructor) {
      return kNameSpaceID_XUL;
    }

    JS_GetPrototype(aCx, proto, &proto);
  }

  return kNameSpaceID_XHTML;
}

// https://html.spec.whatwg.org/multipage/scripting.html#element-definition
void CustomElementRegistry::Define(
    JSContext* aCx, const nsAString& aName,
    CustomElementConstructor& aFunctionConstructor,
    const ElementDefinitionOptions& aOptions, ErrorResult& aRv) {
  JS::Rooted<JSObject*> constructor(aCx, aFunctionConstructor.CallableOrNull());

  // We need to do a dynamic unwrap in order to throw the right exception.  We
  // could probably avoid that if we just threw MSG_NOT_CONSTRUCTOR if unwrap
  // fails.
  //
  // In any case, aCx represents the global we want to be using for the unwrap
  // here.
  JS::Rooted<JSObject*> constructorUnwrapped(
      aCx, js::CheckedUnwrapDynamic(constructor, aCx));
  if (!constructorUnwrapped) {
    // If the caller's compartment does not have permission to access the
    // unwrapped constructor then throw.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  /**
   * 1. If IsConstructor(constructor) is false, then throw a TypeError and abort
   *    these steps.
   */
  if (!JS::IsConstructor(constructorUnwrapped)) {
    aRv.ThrowTypeError<MSG_NOT_CONSTRUCTOR>(
        NS_LITERAL_STRING("Argument 2 of CustomElementRegistry.define"));
    return;
  }

  int32_t nameSpaceID = InferNamespace(aCx, constructor);

  /**
   * 2. If name is not a valid custom element name, then throw a "SyntaxError"
   *    DOMException and abort these steps.
   */
  Document* doc = mWindow->GetExtantDoc();
  RefPtr<nsAtom> nameAtom(NS_Atomize(aName));
  if (!nsContentUtils::IsCustomElementName(nameAtom, nameSpaceID)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  /**
   * 3. If this CustomElementRegistry contains an entry with name name, then
   *    throw a "NotSupportedError" DOMException and abort these steps.
   */
  if (mCustomDefinitions.GetWeak(nameAtom)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  /**
   * 4. If this CustomElementRegistry contains an entry with constructor
   * constructor, then throw a "NotSupportedError" DOMException and abort these
   * steps.
   */
  const auto& ptr = mConstructors.lookup(constructorUnwrapped);
  if (ptr) {
    MOZ_ASSERT(mCustomDefinitions.GetWeak(ptr->value()),
               "Definition must be found in mCustomDefinitions");
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  /**
   * 5. Let localName be name.
   * 6. Let extends be the value of the extends member of options, or null if
   *    no such member exists.
   * 7. If extends is not null, then:
   *    1. If extends is a valid custom element name, then throw a
   *       "NotSupportedError" DOMException.
   *    2. If the element interface for extends and the HTML namespace is
   *       HTMLUnknownElement (e.g., if extends does not indicate an element
   *       definition in this specification), then throw a "NotSupportedError"
   *       DOMException.
   *    3. Set localName to extends.
   *
   * Special note for XUL elements:
   *
   * For step 7.1, we'll subject XUL to the same rules as HTML, so that a
   * custom built-in element will not be extending from a dashed name.
   * Step 7.2 is disregarded. But, we do check if the name is a dashed name
   * (i.e. step 2) given that there is no reason for a custom built-in element
   * type to take on a non-dashed name.
   * This also ensures the name of the built-in custom element type can never
   * be the same as the built-in element name, so we don't break the assumption
   * elsewhere.
   */
  nsAutoString localName(aName);
  if (aOptions.mExtends.WasPassed()) {
    RefPtr<nsAtom> extendsAtom(NS_Atomize(aOptions.mExtends.Value()));
    if (nsContentUtils::IsCustomElementName(extendsAtom, kNameSpaceID_XHTML)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }

    if (nameSpaceID == kNameSpaceID_XHTML) {
      // bgsound and multicol are unknown html element.
      int32_t tag = nsHTMLTags::CaseSensitiveAtomTagToId(extendsAtom);
      if (tag == eHTMLTag_userdefined || tag == eHTMLTag_bgsound ||
          tag == eHTMLTag_multicol) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return;
      }
    } else {  // kNameSpaceID_XUL
      // As stated above, ensure the name of the customized built-in element
      // (the one that goes to the |is| attribute) is a dashed name.
      if (!nsContentUtils::IsNameWithDash(nameAtom)) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return;
      }
    }

    localName.Assign(aOptions.mExtends.Value());
  }

  /**
   * 8. If this CustomElementRegistry's element definition is running flag is
   * set, then throw a "NotSupportedError" DOMException and abort these steps.
   */
  if (mIsCustomDefinitionRunning) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  auto callbacksHolder = MakeUnique<LifecycleCallbacks>();
  nsTArray<RefPtr<nsAtom>> observedAttributes;
  {  // Set mIsCustomDefinitionRunning.
    /**
     * 9. Set this CustomElementRegistry's element definition is running flag.
     */
    AutoSetRunningFlag as(this);

    /**
     * 10.1. Let prototype be Get(constructor, "prototype"). Rethrow any
     * exceptions.
     */
    // The .prototype on the constructor passed could be an "expando" of a
    // wrapper. So we should get it from wrapper instead of the underlying
    // object.
    JS::Rooted<JS::Value> prototype(aCx);
    if (!JS_GetProperty(aCx, constructor, "prototype", &prototype)) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    /**
     * 10.2. If Type(prototype) is not Object, then throw a TypeError exception.
     */
    if (!prototype.isObject()) {
      aRv.ThrowTypeError<MSG_NOT_OBJECT>(
          NS_LITERAL_STRING("constructor.prototype"));
      return;
    }

    /**
     * 10.3. Let lifecycleCallbacks be a map with the four keys
     *       "connectedCallback", "disconnectedCallback", "adoptedCallback", and
     *       "attributeChangedCallback", each of which belongs to an entry whose
     *       value is null. The 'getCustomInterface' callback is also included
     *       for chrome usage.
     * 10.4. For each of the four keys callbackName in lifecycleCallbacks:
     *       1. Let callbackValue be Get(prototype, callbackName). Rethrow any
     *          exceptions.
     *       2. If callbackValue is not undefined, then set the value of the
     *          entry in lifecycleCallbacks with key callbackName to the result
     *          of converting callbackValue to the Web IDL Function callback
     * type. Rethrow any exceptions from the conversion.
     */
    if (!callbacksHolder->Init(aCx, prototype)) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    /**
     * 10.5. Let observedAttributes be an empty sequence<DOMString>.
     * 10.6. If the value of the entry in lifecycleCallbacks with key
     *       "attributeChangedCallback" is not null, then:
     *       1. Let observedAttributesIterable be Get(constructor,
     *          "observedAttributes"). Rethrow any exceptions.
     *       2. If observedAttributesIterable is not undefined, then set
     *          observedAttributes to the result of converting
     *          observedAttributesIterable to a sequence<DOMString>. Rethrow
     *          any exceptions from the conversion.
     */
    if (callbacksHolder->mAttributeChangedCallback.WasPassed()) {
      JS::Rooted<JS::Value> observedAttributesIterable(aCx);

      if (!JS_GetProperty(aCx, constructor, "observedAttributes",
                          &observedAttributesIterable)) {
        aRv.NoteJSContextException(aCx);
        return;
      }

      if (!observedAttributesIterable.isUndefined()) {
        if (!observedAttributesIterable.isObject()) {
          aRv.ThrowTypeError<MSG_NOT_SEQUENCE>(
              NS_LITERAL_STRING("observedAttributes"));
          return;
        }

        JS::ForOfIterator iter(aCx);
        if (!iter.init(observedAttributesIterable,
                       JS::ForOfIterator::AllowNonIterable)) {
          aRv.NoteJSContextException(aCx);
          return;
        }

        if (!iter.valueIsIterable()) {
          aRv.ThrowTypeError<MSG_NOT_SEQUENCE>(
              NS_LITERAL_STRING("observedAttributes"));
          return;
        }

        JS::Rooted<JS::Value> attribute(aCx);
        while (true) {
          bool done;
          if (!iter.next(&attribute, &done)) {
            aRv.NoteJSContextException(aCx);
            return;
          }
          if (done) {
            break;
          }

          nsAutoString attrStr;
          if (!ConvertJSValueToString(aCx, attribute, eStringify, eStringify,
                                      attrStr)) {
            aRv.NoteJSContextException(aCx);
            return;
          }

          if (!observedAttributes.AppendElement(NS_Atomize(attrStr))) {
            aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
          }
        }
      }
    }
  }  // Unset mIsCustomDefinitionRunning

  /**
   * 11. Let definition be a new custom element definition with name name,
   *     local name localName, constructor constructor, prototype prototype,
   *     observed attributes observedAttributes, and lifecycle callbacks
   *     lifecycleCallbacks.
   */
  // Associate the definition with the custom element.
  RefPtr<nsAtom> localNameAtom(NS_Atomize(localName));

  /**
   * 12. Add definition to this CustomElementRegistry.
   */
  if (!mConstructors.put(constructorUnwrapped, nameAtom)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<CustomElementDefinition> definition = new CustomElementDefinition(
      nameAtom, localNameAtom, nameSpaceID, &aFunctionConstructor,
      std::move(observedAttributes), std::move(callbacksHolder));

  CustomElementDefinition* def = definition.get();
  mCustomDefinitions.Put(nameAtom, definition.forget());

  MOZ_ASSERT(mCustomDefinitions.Count() == mConstructors.count(),
             "Number of entries should be the same");

  /**
   * 13. 14. 15. Upgrade candidates
   */
  UpgradeCandidates(nameAtom, def, aRv);

  /**
   * 16. If this CustomElementRegistry's when-defined promise map contains an
   *     entry with key name:
   *     1. Let promise be the value of that entry.
   *     2. Resolve promise with undefined.
   *     3. Delete the entry with key name from this CustomElementRegistry's
   *        when-defined promise map.
   */
  RefPtr<Promise> promise;
  mWhenDefinedPromiseMap.Remove(nameAtom, getter_AddRefs(promise));
  if (promise) {
    promise->MaybeResolveWithUndefined();
  }

  // Dispatch a "customelementdefined" event for DevTools.
  {
    JSString* nameJsStr =
        JS_NewUCStringCopyN(aCx, aName.BeginReading(), aName.Length());

    JS::Rooted<JS::Value> detail(aCx, JS::StringValue(nameJsStr));
    RefPtr<CustomEvent> event = NS_NewDOMCustomEvent(doc, nullptr, nullptr);
    event->InitCustomEvent(aCx, NS_LITERAL_STRING("customelementdefined"),
                           /* CanBubble */ true,
                           /* Cancelable */ true, detail);
    event->SetTrusted(true);

    AsyncEventDispatcher* dispatcher = new AsyncEventDispatcher(doc, event);
    dispatcher->mOnlyChromeDispatch = ChromeOnlyDispatch::eYes;

    dispatcher->PostDOMEvent();
  }

  /**
   * Clean-up mElementCreationCallbacks (if it exists)
   */
  mElementCreationCallbacks.Remove(nameAtom);
}

void CustomElementRegistry::SetElementCreationCallback(
    const nsAString& aName, CustomElementCreationCallback& aCallback,
    ErrorResult& aRv) {
  RefPtr<nsAtom> nameAtom(NS_Atomize(aName));
  if (mElementCreationCallbacks.GetWeak(nameAtom) ||
      mCustomDefinitions.GetWeak(nameAtom)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  RefPtr<CustomElementCreationCallback> callback = &aCallback;
  mElementCreationCallbacks.Put(nameAtom, callback.forget());
  return;
}

void CustomElementRegistry::Upgrade(nsINode& aRoot) {
  for (nsINode* node : ShadowIncludingTreeIterator(aRoot)) {
    Element* element = Element::FromNode(node);
    if (!element) {
      continue;
    }

    CustomElementData* ceData = element->GetCustomElementData();
    if (ceData) {
      NodeInfo* nodeInfo = element->NodeInfo();
      nsAtom* typeAtom = ceData->GetCustomElementType();
      CustomElementDefinition* definition =
          nsContentUtils::LookupCustomElementDefinition(
              nodeInfo->GetDocument(), nodeInfo->NameAtom(),
              nodeInfo->NamespaceID(), typeAtom);
      if (definition) {
        nsContentUtils::EnqueueUpgradeReaction(element, definition);
      }
    }
  }
}

void CustomElementRegistry::Get(JSContext* aCx, const nsAString& aName,
                                JS::MutableHandle<JS::Value> aRetVal) {
  RefPtr<nsAtom> nameAtom(NS_Atomize(aName));
  CustomElementDefinition* data = mCustomDefinitions.GetWeak(nameAtom);

  if (!data) {
    aRetVal.setUndefined();
    return;
  }

  aRetVal.setObject(*data->mConstructor->Callback(aCx));
}

already_AddRefed<Promise> CustomElementRegistry::WhenDefined(
    const nsAString& aName, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  RefPtr<Promise> promise = Promise::Create(global, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<nsAtom> nameAtom(NS_Atomize(aName));
  Document* doc = mWindow->GetExtantDoc();
  uint32_t nameSpaceID =
      doc ? doc->GetDefaultNamespaceID() : kNameSpaceID_XHTML;
  if (!nsContentUtils::IsCustomElementName(nameAtom, nameSpaceID)) {
    promise->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    return promise.forget();
  }

  if (mCustomDefinitions.GetWeak(nameAtom)) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return promise.forget();
  }

  auto entry = mWhenDefinedPromiseMap.LookupForAdd(nameAtom);
  if (entry) {
    promise = entry.Data();
  } else {
    entry.OrInsert([&promise]() { return promise; });
  }

  return promise.forget();
}

namespace {

MOZ_CAN_RUN_SCRIPT
static void DoUpgrade(Element* aElement, CustomElementConstructor* aConstructor,
                      ErrorResult& aRv) {
  JS::Rooted<JS::Value> constructResult(RootingCx());
  // Rethrow the exception since it might actually throw the exception from the
  // upgrade steps back out to the caller of document.createElement.
  aConstructor->Construct(&constructResult, aRv, "Custom Element Upgrade",
                          CallbackFunction::eRethrowExceptions);
  if (aRv.Failed()) {
    return;
  }

  Element* element;
  // constructResult is an ObjectValue because construction with a callback
  // always forms the return value from a JSObject.
  if (NS_FAILED(UNWRAP_OBJECT(Element, &constructResult, element)) ||
      element != aElement) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
}

}  // anonymous namespace

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
/* static */
void CustomElementRegistry::Upgrade(Element* aElement,
                                    CustomElementDefinition* aDefinition,
                                    ErrorResult& aRv) {
  RefPtr<CustomElementData> data = aElement->GetCustomElementData();
  MOZ_ASSERT(data, "CustomElementData should exist");

  // Step 1 and step 2.
  if (data->mState == CustomElementData::State::eCustom ||
      data->mState == CustomElementData::State::eFailed) {
    return;
  }

  // Step 3.
  if (!aDefinition->mObservedAttributes.IsEmpty()) {
    uint32_t count = aElement->GetAttrCount();
    for (uint32_t i = 0; i < count; i++) {
      mozilla::dom::BorrowedAttrInfo info = aElement->GetAttrInfoAt(i);

      const nsAttrName* name = info.mName;
      nsAtom* attrName = name->LocalName();

      if (aDefinition->IsInObservedAttributeList(attrName)) {
        int32_t namespaceID = name->NamespaceID();
        nsAutoString attrValue, namespaceURI;
        info.mValue->ToString(attrValue);
        nsContentUtils::NameSpaceManager()->GetNameSpaceURI(namespaceID,
                                                            namespaceURI);

        LifecycleCallbackArgs args = {
            nsDependentAtomString(attrName), VoidString(), attrValue,
            (namespaceURI.IsEmpty() ? VoidString() : namespaceURI)};
        nsContentUtils::EnqueueLifecycleCallback(
            Document::eAttributeChanged, aElement, &args, nullptr, aDefinition);
      }
    }
  }

  // Step 4.
  if (aElement->IsInComposedDoc()) {
    nsContentUtils::EnqueueLifecycleCallback(Document::eConnected, aElement,
                                             nullptr, nullptr, aDefinition);
  }

  // Step 5.
  AutoConstructionStackEntry acs(aDefinition->mConstructionStack, aElement);

  // Step 6 and step 7.
  DoUpgrade(aElement, MOZ_KnownLive(aDefinition->mConstructor), aRv);
  if (aRv.Failed()) {
    data->mState = CustomElementData::State::eFailed;
    // Empty element's custom element reaction queue.
    data->mReactionQueue.Clear();
    return;
  }

  // Step 8.
  data->mState = CustomElementData::State::eCustom;
  aElement->SetDefined(true);

  // Step 9.
  aElement->SetCustomElementDefinition(aDefinition);
}

already_AddRefed<nsISupports> CustomElementRegistry::CallGetCustomInterface(
    Element* aElement, const nsIID& aIID) {
  MOZ_ASSERT(aElement);

  if (!nsContentUtils::IsChromeDoc(aElement->OwnerDoc())) {
    return nullptr;
  }

  // Try to get our GetCustomInterfaceCallback callback.
  CustomElementDefinition* definition = aElement->GetCustomElementDefinition();
  if (!definition || !definition->mCallbacks ||
      !definition->mCallbacks->mGetCustomInterfaceCallback.WasPassed() ||
      (definition->mLocalName != aElement->NodeInfo()->NameAtom())) {
    return nullptr;
  }
  LifecycleGetCustomInterfaceCallback* func =
      definition->mCallbacks->mGetCustomInterfaceCallback.Value();

  // Initialize a AutoJSAPI to enter the compartment of the callback.
  AutoJSAPI jsapi;
  JS::RootedObject funcGlobal(RootingCx(), func->CallbackGlobalOrNull());
  if (!funcGlobal || !jsapi.Init(funcGlobal)) {
    return nullptr;
  }

  // Grab our JSContext.
  JSContext* cx = jsapi.cx();

  // Convert our IID to a JSValue to call our callback.
  JS::RootedValue jsiid(cx);
  if (!xpc::ID2JSValue(cx, aIID, &jsiid)) {
    return nullptr;
  }

  JS::RootedObject customInterface(cx);
  func->Call(aElement, jsiid, &customInterface);
  if (!customInterface) {
    return nullptr;
  }

  // Wrap our JSObject into a nsISupports through XPConnect
  nsCOMPtr<nsISupports> wrapper;
  nsresult rv = nsContentUtils::XPConnect()->WrapJSAggregatedToNative(
      aElement, cx, customInterface, aIID, getter_AddRefs(wrapper));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return wrapper.forget();
}

//-----------------------------------------------------
// CustomElementReactionsStack

void CustomElementReactionsStack::CreateAndPushElementQueue() {
  MOZ_ASSERT(mRecursionDepth);
  MOZ_ASSERT(!mIsElementQueuePushedForCurrentRecursionDepth);

  // Push a new element queue onto the custom element reactions stack.
  mReactionsStack.AppendElement(MakeUnique<ElementQueue>());
  mIsElementQueuePushedForCurrentRecursionDepth = true;
}

void CustomElementReactionsStack::PopAndInvokeElementQueue() {
  MOZ_ASSERT(mRecursionDepth);
  MOZ_ASSERT(mIsElementQueuePushedForCurrentRecursionDepth);
  MOZ_ASSERT(!mReactionsStack.IsEmpty(), "Reaction stack shouldn't be empty");

  // Pop the element queue from the custom element reactions stack,
  // and invoke custom element reactions in that queue.
  const uint32_t lastIndex = mReactionsStack.Length() - 1;
  ElementQueue* elementQueue = mReactionsStack.ElementAt(lastIndex).get();
  // Check element queue size in order to reduce function call overhead.
  if (!elementQueue->IsEmpty()) {
    // It is still not clear what error reporting will look like in custom
    // element, see https://github.com/w3c/webcomponents/issues/635.
    // We usually report the error to entry global in gecko, so just follow the
    // same behavior here.
    // This may be null if it's called from parser, see the case of
    // attributeChangedCallback in
    // https://html.spec.whatwg.org/multipage/parsing.html#create-an-element-for-the-token
    // In that case, the exception of callback reactions will be automatically
    // reported in CallSetup.
    nsIGlobalObject* global = GetEntryGlobal();
    InvokeReactions(elementQueue, MOZ_KnownLive(global));
  }

  // InvokeReactions() might create other custom element reactions, but those
  // new reactions should be already consumed and removed at this point.
  MOZ_ASSERT(
      lastIndex == mReactionsStack.Length() - 1,
      "reactions created by InvokeReactions() should be consumed and removed");

  mReactionsStack.RemoveElementAt(lastIndex);
  mIsElementQueuePushedForCurrentRecursionDepth = false;
}

void CustomElementReactionsStack::EnqueueUpgradeReaction(
    Element* aElement, CustomElementDefinition* aDefinition) {
  Enqueue(aElement, new CustomElementUpgradeReaction(aDefinition));
}

void CustomElementReactionsStack::EnqueueCallbackReaction(
    Element* aElement,
    UniquePtr<CustomElementCallback> aCustomElementCallback) {
  Enqueue(aElement,
          new CustomElementCallbackReaction(std::move(aCustomElementCallback)));
}

void CustomElementReactionsStack::Enqueue(Element* aElement,
                                          CustomElementReaction* aReaction) {
  RefPtr<CustomElementData> elementData = aElement->GetCustomElementData();
  MOZ_ASSERT(elementData, "CustomElementData should exist");

  if (mRecursionDepth) {
    // If the element queue is not created for current recursion depth, create
    // and push an element queue to reactions stack first.
    if (!mIsElementQueuePushedForCurrentRecursionDepth) {
      CreateAndPushElementQueue();
    }

    MOZ_ASSERT(!mReactionsStack.IsEmpty());
    // Add element to the current element queue.
    mReactionsStack.LastElement()->AppendElement(aElement);
    elementData->mReactionQueue.AppendElement(aReaction);
    return;
  }

  // If the custom element reactions stack is empty, then:
  // Add element to the backup element queue.
  MOZ_ASSERT(mReactionsStack.IsEmpty(),
             "custom element reactions stack should be empty");
  mBackupQueue.AppendElement(aElement);
  elementData->mReactionQueue.AppendElement(aReaction);

  if (mIsBackupQueueProcessing) {
    return;
  }

  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();
  RefPtr<BackupQueueMicroTask> bqmt = new BackupQueueMicroTask(this);
  context->DispatchToMicroTask(bqmt.forget());
}

void CustomElementReactionsStack::InvokeBackupQueue() {
  // Check backup queue size in order to reduce function call overhead.
  if (!mBackupQueue.IsEmpty()) {
    // Upgrade reactions won't be scheduled in backup queue and the exception of
    // callback reactions will be automatically reported in CallSetup.
    // If the reactions are invoked from backup queue (in microtask check
    // point), we don't need to pass global object for error reporting.
    InvokeReactions(&mBackupQueue, nullptr);
  }
  MOZ_ASSERT(
      mBackupQueue.IsEmpty(),
      "There are still some reactions in BackupQueue not being consumed!?!");
}

void CustomElementReactionsStack::InvokeReactions(ElementQueue* aElementQueue,
                                                  nsIGlobalObject* aGlobal) {
  // This is used for error reporting.
  Maybe<AutoEntryScript> aes;
  if (aGlobal) {
    aes.emplace(aGlobal, "custom elements reaction invocation");
  }

  // Note: It's possible to re-enter this method.
  for (uint32_t i = 0; i < aElementQueue->Length(); ++i) {
    Element* element = aElementQueue->ElementAt(i);
    // ElementQueue hold a element's strong reference, it should not be a
    // nullptr.
    MOZ_ASSERT(element);

    RefPtr<CustomElementData> elementData = element->GetCustomElementData();
    if (!elementData || !element->GetOwnerGlobal()) {
      // This happens when the document is destroyed and the element is already
      // unlinked, no need to fire the callbacks in this case.
      continue;
    }

    auto& reactions = elementData->mReactionQueue;
    for (uint32_t j = 0; j < reactions.Length(); ++j) {
      // Transfer the ownership of the entry due to reentrant invocation of
      // this function.
      auto reaction(std::move(reactions.ElementAt(j)));
      if (reaction) {
        if (!aGlobal && reaction->IsUpgradeReaction()) {
          nsIGlobalObject* global = element->GetOwnerGlobal();
          MOZ_ASSERT(!aes);
          aes.emplace(global, "custom elements reaction invocation");
        }
        ErrorResult rv;
        reaction->Invoke(MOZ_KnownLive(element), rv);
        if (aes) {
          JSContext* cx = aes->cx();
          if (rv.MaybeSetPendingException(cx)) {
            aes->ReportException();
          }
          MOZ_ASSERT(!JS_IsExceptionPending(cx));
          if (!aGlobal && reaction->IsUpgradeReaction()) {
            aes.reset();
          }
        }
        MOZ_ASSERT(!rv.Failed());
      }
    }
    reactions.Clear();
  }
  aElementQueue->Clear();
}

//-----------------------------------------------------
// CustomElementDefinition

NS_IMPL_CYCLE_COLLECTION_CLASS(CustomElementDefinition)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CustomElementDefinition)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConstructor)
  tmp->mCallbacks = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CustomElementDefinition)
  mozilla::dom::LifecycleCallbacks* callbacks = tmp->mCallbacks.get();

  if (callbacks->mAttributeChangedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                       "mCallbacks->mAttributeChangedCallback");
    cb.NoteXPCOMChild(callbacks->mAttributeChangedCallback.Value());
  }

  if (callbacks->mConnectedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCallbacks->mConnectedCallback");
    cb.NoteXPCOMChild(callbacks->mConnectedCallback.Value());
  }

  if (callbacks->mDisconnectedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCallbacks->mDisconnectedCallback");
    cb.NoteXPCOMChild(callbacks->mDisconnectedCallback.Value());
  }

  if (callbacks->mAdoptedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCallbacks->mAdoptedCallback");
    cb.NoteXPCOMChild(callbacks->mAdoptedCallback.Value());
  }

  if (callbacks->mGetCustomInterfaceCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        cb, "mCallbacks->mGetCustomInterfaceCallback");
    cb.NoteXPCOMChild(callbacks->mGetCustomInterfaceCallback.Value());
  }

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mConstructor");
  cb.NoteXPCOMChild(tmp->mConstructor);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CustomElementDefinition)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CustomElementDefinition, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CustomElementDefinition, Release)

CustomElementDefinition::CustomElementDefinition(
    nsAtom* aType, nsAtom* aLocalName, int32_t aNamespaceID,
    CustomElementConstructor* aConstructor,
    nsTArray<RefPtr<nsAtom>>&& aObservedAttributes,
    UniquePtr<LifecycleCallbacks>&& aCallbacks)
    : mType(aType),
      mLocalName(aLocalName),
      mNamespaceID(aNamespaceID),
      mConstructor(aConstructor),
      mObservedAttributes(std::move(aObservedAttributes)),
      mCallbacks(std::move(aCallbacks)) {}

}  // namespace dom
}  // namespace mozilla
