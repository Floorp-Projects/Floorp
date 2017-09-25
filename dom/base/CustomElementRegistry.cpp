/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CustomElementRegistry.h"

#include "mozilla/dom/CustomElementRegistryBinding.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/WebComponentsBinding.h"
#include "mozilla/dom/DocGroup.h"
#include "nsHTMLTags.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {

void
CustomElementCallback::Call()
{
  IgnoredErrorResult rv;
  switch (mType) {
    case nsIDocument::eCreated:
    {
      // For the duration of this callback invocation, the element is being created
      // flag must be set to true.
      mOwnerData->mElementIsBeingCreated = true;

      // The callback hasn't actually been invoked yet, but we need to flip
      // this now in order to enqueue the attached callback. This is a spec
      // bug (w3c bug 27437).
      mOwnerData->mCreatedCallbackInvoked = true;

      // If ELEMENT is in a document and this document has a browsing context,
      // enqueue attached callback for ELEMENT.
      nsIDocument* document = mThisObject->GetComposedDoc();
      if (document && document->GetDocShell()) {
        NodeInfo* ni = mThisObject->NodeInfo();
        nsDependentAtomString extType(mOwnerData->mType);

        // We need to do this because at this point, CustomElementDefinition is
        // not set to CustomElementData yet, so EnqueueLifecycleCallback will
        // fail to find the CE definition for this custom element.
        // This will go away eventually since there is no created callback in v1.
        CustomElementDefinition* definition =
          nsContentUtils::LookupCustomElementDefinition(document,
            ni->LocalName(), ni->NamespaceID(),
            extType.IsEmpty() ? nullptr : &extType);

        nsContentUtils::EnqueueLifecycleCallback(
          document, nsIDocument::eAttached, mThisObject, nullptr, definition);
      }

      static_cast<LifecycleCreatedCallback *>(mCallback.get())->Call(mThisObject, rv);
      mOwnerData->mElementIsBeingCreated = false;
      break;
    }
    case nsIDocument::eAttached:
      static_cast<LifecycleAttachedCallback *>(mCallback.get())->Call(mThisObject, rv);
      break;
    case nsIDocument::eDetached:
      static_cast<LifecycleDetachedCallback *>(mCallback.get())->Call(mThisObject, rv);
      break;
    case nsIDocument::eAttributeChanged:
      static_cast<LifecycleAttributeChangedCallback *>(mCallback.get())->Call(mThisObject,
        mArgs.name, mArgs.oldValue, mArgs.newValue, mArgs.namespaceURI, rv);
      break;
  }
}

void
CustomElementCallback::Traverse(nsCycleCollectionTraversalCallback& aCb) const
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mThisObject");
  aCb.NoteXPCOMChild(mThisObject);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "mCallback");
  aCb.NoteXPCOMChild(mCallback);
}

CustomElementCallback::CustomElementCallback(Element* aThisObject,
                                             nsIDocument::ElementCallbackType aCallbackType,
                                             mozilla::dom::CallbackFunction* aCallback,
                                             CustomElementData* aOwnerData)
  : mThisObject(aThisObject),
    mCallback(aCallback),
    mType(aCallbackType),
    mOwnerData(aOwnerData)
{
}
//-----------------------------------------------------
// CustomElementConstructor

already_AddRefed<Element>
CustomElementConstructor::Construct(const char* aExecutionReason,
                                    ErrorResult& aRv)
{
  CallSetup s(this, aRv, aExecutionReason,
              CallbackFunction::eRethrowExceptions);

  JSContext* cx = s.GetContext();
  if (!cx) {
    MOZ_ASSERT(aRv.Failed());
    return nullptr;
  }

  JS::Rooted<JSObject*> result(cx);
  JS::Rooted<JS::Value> constructor(cx, JS::ObjectValue(*mCallback));
  if (!JS::Construct(cx, constructor, JS::HandleValueArray::empty(), &result)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<Element> element;
  if (NS_FAILED(UNWRAP_OBJECT(Element, &result, element))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return element.forget();
}
//-----------------------------------------------------
// CustomElementData

CustomElementData::CustomElementData(nsIAtom* aType)
  : CustomElementData(aType, CustomElementData::State::eUndefined)
{
}

CustomElementData::CustomElementData(nsIAtom* aType, State aState)
  : mType(aType)
  , mElementIsBeingCreated(false)
  , mCreatedCallbackInvoked(true)
  , mState(aState)
{
}

//-----------------------------------------------------
// CustomElementRegistry

namespace {

class MOZ_RAII AutoConstructionStackEntry final
{
public:
  AutoConstructionStackEntry(nsTArray<RefPtr<nsGenericHTMLElement>>& aStack,
                             nsGenericHTMLElement* aElement)
    : mStack(aStack)
  {
    mIndex = mStack.Length();
    mStack.AppendElement(aElement);
  }

  ~AutoConstructionStackEntry()
  {
    MOZ_ASSERT(mIndex == mStack.Length() - 1,
               "Removed element should be the last element");
    mStack.RemoveElementAt(mIndex);
  }

private:
  nsTArray<RefPtr<nsGenericHTMLElement>>& mStack;
  uint32_t mIndex;
};

} // namespace anonymous

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_CLASS(CustomElementRegistry)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CustomElementRegistry)
  tmp->mConstructors.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCustomDefinitions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWhenDefinedPromiseMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CustomElementRegistry)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCustomDefinitions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWhenDefinedPromiseMap)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CustomElementRegistry)
  for (auto iter = tmp->mCustomDefinitions.Iter(); !iter.Done(); iter.Next()) {
    aCallbacks.Trace(&iter.UserData()->mPrototype,
                     "mCustomDefinitions prototype",
                     aClosure);
  }

  for (ConstructorMap::Enum iter(tmp->mConstructors); !iter.empty(); iter.popFront()) {
    aCallbacks.Trace(&iter.front().mutableKey(),
                     "mConstructors key",
                     aClosure);
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
 : mWindow(aWindow)
 , mIsCustomDefinitionRunning(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());
  MOZ_ALWAYS_TRUE(mConstructors.init());

  mozilla::HoldJSObjects(this);
}

CustomElementRegistry::~CustomElementRegistry()
{
  mozilla::DropJSObjects(this);
}

CustomElementDefinition*
CustomElementRegistry::LookupCustomElementDefinition(const nsAString& aLocalName,
                                                     const nsAString* aIs) const
{
  nsCOMPtr<nsIAtom> localNameAtom = NS_Atomize(aLocalName);
  nsCOMPtr<nsIAtom> typeAtom = aIs ? NS_Atomize(*aIs) : localNameAtom;

  CustomElementDefinition* data = mCustomDefinitions.GetWeak(typeAtom);
  if (data && data->mLocalName == localNameAtom) {
    return data;
  }

  return nullptr;
}

CustomElementDefinition*
CustomElementRegistry::LookupCustomElementDefinition(JSContext* aCx,
                                                     JSObject* aConstructor) const
{
  JS::Rooted<JSObject*> constructor(aCx, js::CheckedUnwrap(aConstructor));

  const auto& ptr = mConstructors.lookup(constructor);
  if (!ptr) {
    return nullptr;
  }

  CustomElementDefinition* definition = mCustomDefinitions.GetWeak(ptr->value());
  MOZ_ASSERT(definition, "Definition must be found in mCustomDefinitions");

  return definition;
}

void
CustomElementRegistry::RegisterUnresolvedElement(Element* aElement, nsIAtom* aTypeName)
{
  mozilla::dom::NodeInfo* info = aElement->NodeInfo();

  // Candidate may be a custom element through extension,
  // in which case the custom element type name will not
  // match the element tag name. e.g. <button is="x-button">.
  nsCOMPtr<nsIAtom> typeName = aTypeName;
  if (!typeName) {
    typeName = info->NameAtom();
  }

  if (mCustomDefinitions.GetWeak(typeName)) {
    return;
  }

  nsTArray<nsWeakPtr>* unresolved = mCandidatesMap.LookupOrAdd(typeName);
  nsWeakPtr* elem = unresolved->AppendElement();
  *elem = do_GetWeakReference(aElement);
  aElement->AddStates(NS_EVENT_STATE_UNRESOLVED);
}

void
CustomElementRegistry::SetupCustomElement(Element* aElement,
                                          const nsAString* aTypeExtension)
{
  nsCOMPtr<nsIAtom> tagAtom = aElement->NodeInfo()->NameAtom();
  nsCOMPtr<nsIAtom> typeAtom = aTypeExtension ?
    NS_Atomize(*aTypeExtension) : tagAtom;

  if (aTypeExtension && !aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::is)) {
    // Custom element setup in the parser happens after the "is"
    // attribute is added.
    aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::is, *aTypeExtension, true);
  }

  // SetupCustomElement() should be called with an element that don't have
  // CustomElementData setup, if not we will hit the assertion in
  // SetCustomElementData().
  aElement->SetCustomElementData(new CustomElementData(typeAtom));

  CustomElementDefinition* definition = LookupCustomElementDefinition(
    aElement->NodeInfo()->LocalName(), aTypeExtension);

  if (!definition) {
    // The type extension doesn't exist in the registry,
    // thus we don't need to enqueue callback or adjust
    // the "is" attribute, but it is possibly an upgrade candidate.
    RegisterUnresolvedElement(aElement, typeAtom);
    return;
  }

  if (definition->mLocalName != tagAtom) {
    // The element doesn't match the local name for the
    // definition, thus the element isn't a custom element
    // and we don't need to do anything more.
    return;
  }

  // Enqueuing the created callback will set the CustomElementData on the
  // element, causing prototype swizzling to occur in Element::WrapObject.
  // We make it synchronously for createElement/createElementNS in order to
  // pass tests. It'll be removed when we deprecate custom elements v0.
  SyncInvokeReactions(nsIDocument::eCreated, aElement, definition);
}

UniquePtr<CustomElementCallback>
CustomElementRegistry::CreateCustomElementCallback(
  nsIDocument::ElementCallbackType aType, Element* aCustomElement,
  LifecycleCallbackArgs* aArgs, CustomElementDefinition* aDefinition)
{
  MOZ_ASSERT(aDefinition, "CustomElementDefinition should not be null");

  RefPtr<CustomElementData> elementData = aCustomElement->GetCustomElementData();
  MOZ_ASSERT(elementData, "CustomElementData should exist");

  // Let CALLBACK be the callback associated with the key NAME in CALLBACKS.
  CallbackFunction* func = nullptr;
  switch (aType) {
    case nsIDocument::eCreated:
      if (aDefinition->mCallbacks->mCreatedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mCreatedCallback.Value();
      }
      break;

    case nsIDocument::eAttached:
      if (aDefinition->mCallbacks->mAttachedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mAttachedCallback.Value();
      }
      break;

    case nsIDocument::eDetached:
      if (aDefinition->mCallbacks->mDetachedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mDetachedCallback.Value();
      }
      break;

    case nsIDocument::eAttributeChanged:
      if (aDefinition->mCallbacks->mAttributeChangedCallback.WasPassed()) {
        func = aDefinition->mCallbacks->mAttributeChangedCallback.Value();
      }
      break;
  }

  // If there is no such callback, stop.
  if (!func) {
    return nullptr;
  }

  if (aType == nsIDocument::eCreated) {
    elementData->mCreatedCallbackInvoked = false;
  } else if (!elementData->mCreatedCallbackInvoked) {
    // Callbacks other than created callback must not be enqueued
    // until after the created callback has been invoked.
    return nullptr;
  }

  // Add CALLBACK to ELEMENT's callback queue.
  auto callback =
    MakeUnique<CustomElementCallback>(aCustomElement, aType, func, elementData);

  if (aArgs) {
    callback->SetArgs(*aArgs);
  }

  return Move(callback);
}

void
CustomElementRegistry::SyncInvokeReactions(nsIDocument::ElementCallbackType aType,
                                           Element* aCustomElement,
                                           CustomElementDefinition* aDefinition)
{
  auto callback = CreateCustomElementCallback(aType, aCustomElement, nullptr,
                                              aDefinition);
  if (!callback) {
    return;
  }

  UniquePtr<CustomElementReaction> reaction(Move(
    MakeUnique<CustomElementCallbackReaction>(this, aDefinition,
                                              Move(callback))));

  RefPtr<SyncInvokeReactionRunnable> runnable =
    new SyncInvokeReactionRunnable(Move(reaction), aCustomElement);

  nsContentUtils::AddScriptRunner(runnable);
}

void
CustomElementRegistry::EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                                Element* aCustomElement,
                                                LifecycleCallbackArgs* aArgs,
                                                CustomElementDefinition* aDefinition)
{
  CustomElementDefinition* definition = aDefinition;
  if (!definition) {
    definition = aCustomElement->GetCustomElementDefinition();
    if (!definition ||
        definition->mLocalName != aCustomElement->NodeInfo()->NameAtom()) {
      return;
    }
  }

  auto callback =
    CreateCustomElementCallback(aType, aCustomElement, aArgs, definition);
  if (!callback) {
    return;
  }

  DocGroup* docGroup = mWindow->GetDocGroup();
  if (!docGroup) {
    return;
  }

  if (aType == nsIDocument::eAttributeChanged) {
    nsCOMPtr<nsIAtom> attrName = NS_Atomize(aArgs->name);
    if (definition->mObservedAttributes.IsEmpty() ||
        !definition->mObservedAttributes.Contains(attrName)) {
      return;
    }
  }

  CustomElementReactionsStack* reactionsStack =
    docGroup->CustomElementReactionsStack();
  reactionsStack->EnqueueCallbackReaction(this, aCustomElement, definition,
                                          Move(callback));
}

void
CustomElementRegistry::GetCustomPrototype(nsIAtom* aAtom,
                                          JS::MutableHandle<JSObject*> aPrototype)
{
  mozilla::dom::CustomElementDefinition* definition =
    mCustomDefinitions.GetWeak(aAtom);
  if (definition) {
    aPrototype.set(definition->mPrototype);
  } else {
    aPrototype.set(nullptr);
  }
}

void
CustomElementRegistry::UpgradeCandidates(nsIAtom* aKey,
                                         CustomElementDefinition* aDefinition,
                                         ErrorResult& aRv)
{
  DocGroup* docGroup = mWindow->GetDocGroup();
  if (!docGroup) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // TODO: Bug 1326028 - Upgrade custom element in shadow-including tree order
  nsAutoPtr<nsTArray<nsWeakPtr>> candidates;
  if (mCandidatesMap.Remove(aKey, &candidates)) {
    MOZ_ASSERT(candidates);
    CustomElementReactionsStack* reactionsStack =
      docGroup->CustomElementReactionsStack();
    for (size_t i = 0; i < candidates->Length(); ++i) {
      nsCOMPtr<Element> elem = do_QueryReferent(candidates->ElementAt(i));
      if (!elem) {
        continue;
      }

      reactionsStack->EnqueueUpgradeReaction(this, elem, aDefinition);
    }
  }
}

JSObject*
CustomElementRegistry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CustomElementRegistryBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports* CustomElementRegistry::GetParentObject() const
{
  return mWindow;
}

static const char* kLifeCycleCallbackNames[] = {
  "connectedCallback",
  "disconnectedCallback",
  "adoptedCallback",
  "attributeChangedCallback",
  // The life cycle callbacks from v0 spec.
  "createdCallback",
  "attachedCallback",
  "detachedCallback"
};

static void
CheckLifeCycleCallbacks(JSContext* aCx,
                        JS::Handle<JSObject*> aConstructor,
                        ErrorResult& aRv)
{
  for (size_t i = 0; i < ArrayLength(kLifeCycleCallbackNames); ++i) {
    const char* callbackName = kLifeCycleCallbackNames[i];
    JS::Rooted<JS::Value> callbackValue(aCx);
    if (!JS_GetProperty(aCx, aConstructor, callbackName, &callbackValue)) {
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }
    if (!callbackValue.isUndefined()) {
      if (!callbackValue.isObject()) {
        aRv.ThrowTypeError<MSG_NOT_OBJECT>(NS_ConvertASCIItoUTF16(callbackName));
        return;
      }
      JS::Rooted<JSObject*> callback(aCx, &callbackValue.toObject());
      if (!JS::IsCallable(callback)) {
        aRv.ThrowTypeError<MSG_NOT_CALLABLE>(NS_ConvertASCIItoUTF16(callbackName));
        return;
      }
    }
  }
}

// https://html.spec.whatwg.org/multipage/scripting.html#element-definition
void
CustomElementRegistry::Define(const nsAString& aName,
                              Function& aFunctionConstructor,
                              const ElementDefinitionOptions& aOptions,
                              ErrorResult& aRv)
{
  aRv.MightThrowJSException();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  JSContext *cx = jsapi.cx();
  // Note: No calls that might run JS or trigger CC before this point, or
  // there's a (vanishingly small) chance of our constructor being nulled
  // before we access it.
  JS::Rooted<JSObject*> constructor(cx, aFunctionConstructor.CallableOrNull());

  /**
   * 1. If IsConstructor(constructor) is false, then throw a TypeError and abort
   *    these steps.
   */
  // For now, all wrappers are constructable if they are callable. So we need to
  // unwrap constructor to check it is really constructable.
  JS::Rooted<JSObject*> constructorUnwrapped(cx, js::CheckedUnwrap(constructor));
  if (!constructorUnwrapped) {
    // If the caller's compartment does not have permission to access the
    // unwrapped constructor then throw.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!JS::IsConstructor(constructorUnwrapped)) {
    aRv.ThrowTypeError<MSG_NOT_CONSTRUCTOR>(NS_LITERAL_STRING("Argument 2 of CustomElementRegistry.define"));
    return;
  }

  /**
   * 2. If name is not a valid custom element name, then throw a "SyntaxError"
   *    DOMException and abort these steps.
   */
  nsCOMPtr<nsIAtom> nameAtom(NS_Atomize(aName));
  if (!nsContentUtils::IsCustomElementName(nameAtom)) {
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
   * 4. If this CustomElementRegistry contains an entry with constructor constructor,
   *    then throw a "NotSupportedError" DOMException and abort these steps.
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
   */
  nsAutoString localName(aName);
  if (aOptions.mExtends.WasPassed()) {
    nsCOMPtr<nsIAtom> extendsAtom(NS_Atomize(aOptions.mExtends.Value()));
    if (nsContentUtils::IsCustomElementName(extendsAtom)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }

    // bgsound and multicol are unknown html element.
    int32_t tag = nsHTMLTags::CaseSensitiveAtomTagToId(extendsAtom);
    if (tag == eHTMLTag_userdefined ||
        tag == eHTMLTag_bgsound ||
        tag == eHTMLTag_multicol) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }

    localName.Assign(aOptions.mExtends.Value());
  }

  /**
   * 8. If this CustomElementRegistry's element definition is running flag is set,
   *    then throw a "NotSupportedError" DOMException and abort these steps.
   */
  if (mIsCustomDefinitionRunning) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  JS::Rooted<JSObject*> constructorPrototype(cx);
  nsAutoPtr<LifecycleCallbacks> callbacksHolder(new LifecycleCallbacks());
  nsCOMArray<nsIAtom> observedAttributes;
  { // Set mIsCustomDefinitionRunning.
    /**
     * 9. Set this CustomElementRegistry's element definition is running flag.
     */
    AutoSetRunningFlag as(this);

    { // Enter constructor's compartment.
      /**
       * 10.1. Let prototype be Get(constructor, "prototype"). Rethrow any exceptions.
       */
      JSAutoCompartment ac(cx, constructor);
      JS::Rooted<JS::Value> prototypev(cx);
      // The .prototype on the constructor passed from document.registerElement
      // is the "expando" of a wrapper. So we should get it from wrapper instead
      // instead of underlying object.
      if (!JS_GetProperty(cx, constructor, "prototype", &prototypev)) {
        aRv.StealExceptionFromJSContext(cx);
        return;
      }

      /**
       * 10.2. If Type(prototype) is not Object, then throw a TypeError exception.
       */
      if (!prototypev.isObject()) {
        aRv.ThrowTypeError<MSG_NOT_OBJECT>(NS_LITERAL_STRING("constructor.prototype"));
        return;
      }

      constructorPrototype = &prototypev.toObject();
    } // Leave constructor's compartment.

    JS::Rooted<JSObject*> constructorProtoUnwrapped(cx, js::CheckedUnwrap(constructorPrototype));
    if (!constructorProtoUnwrapped) {
      // If the caller's compartment does not have permission to access the
      // unwrapped prototype then throw.
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    { // Enter constructorProtoUnwrapped's compartment
      JSAutoCompartment ac(cx, constructorProtoUnwrapped);

      /**
       * 10.3. Let lifecycleCallbacks be a map with the four keys
       *       "connectedCallback", "disconnectedCallback", "adoptedCallback", and
       *       "attributeChangedCallback", each of which belongs to an entry whose
       *       value is null.
       * 10.4. For each of the four keys callbackName in lifecycleCallbacks:
       *       1. Let callbackValue be Get(prototype, callbackName). Rethrow any
       *          exceptions.
       *       2. If callbackValue is not undefined, then set the value of the
       *          entry in lifecycleCallbacks with key callbackName to the result
       *          of converting callbackValue to the Web IDL Function callback type.
       *          Rethrow any exceptions from the conversion.
       */
      // Will do the same checking for the life cycle callbacks from v0 spec.
      CheckLifeCycleCallbacks(cx, constructorProtoUnwrapped, aRv);
      if (aRv.Failed()) {
        return;
      }

      // Note: We call the init from the constructorProtoUnwrapped's compartment
      //       here.
      JS::RootedValue rootedv(cx, JS::ObjectValue(*constructorProtoUnwrapped));
      if (!JS_WrapValue(cx, &rootedv) || !callbacksHolder->Init(cx, rootedv)) {
        aRv.StealExceptionFromJSContext(cx);
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
      // TODO: Bug 1293921 - Implement connected/disconnected/adopted/attributeChanged lifecycle callbacks for custom elements
      if (callbacksHolder->mAttributeChangedCallback.WasPassed()) {
        // Enter constructor's compartment.
        JSAutoCompartment ac(cx, constructor);
        JS::Rooted<JS::Value> observedAttributesIterable(cx);

        if (!JS_GetProperty(cx, constructor, "observedAttributes",
                            &observedAttributesIterable)) {
          aRv.StealExceptionFromJSContext(cx);
          return;
        }

        if (!observedAttributesIterable.isUndefined()) {
          JS::ForOfIterator iter(cx);
          if (!iter.init(observedAttributesIterable)) {
            aRv.StealExceptionFromJSContext(cx);
            return;
          }

          JS::Rooted<JS::Value> attribute(cx);
          while (true) {
            bool done;
            if (!iter.next(&attribute, &done)) {
              aRv.StealExceptionFromJSContext(cx);
              return;
            }
            if (done) {
              break;
            }

            JSString *attrJSStr = attribute.toString();
            nsAutoJSString attrStr;
            if (!attrStr.init(cx, attrJSStr)) {
              aRv.StealExceptionFromJSContext(cx);
              return;
            }
            observedAttributes.AppendElement(NS_Atomize(attrStr));
          }
        }
      } // Leave constructor's compartment.
    } // Leave constructorProtoUnwrapped's compartment.
  } // Unset mIsCustomDefinitionRunning

  /**
   * 11. Let definition be a new custom element definition with name name,
   *     local name localName, constructor constructor, prototype prototype,
   *     observed attributes observedAttributes, and lifecycle callbacks
   *     lifecycleCallbacks.
   */
  // Associate the definition with the custom element.
  nsCOMPtr<nsIAtom> localNameAtom(NS_Atomize(localName));
  LifecycleCallbacks* callbacks = callbacksHolder.forget();

  /**
   * 12. Add definition to this CustomElementRegistry.
   */
  if (!mConstructors.put(constructorUnwrapped, nameAtom)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<CustomElementDefinition> definition =
    new CustomElementDefinition(nameAtom,
                                localNameAtom,
                                &aFunctionConstructor,
                                Move(observedAttributes),
                                constructorPrototype,
                                callbacks,
                                0 /* TODO dependent on HTML imports. Bug 877072 */);

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

}

void
CustomElementRegistry::Get(JSContext* aCx, const nsAString& aName,
                           JS::MutableHandle<JS::Value> aRetVal)
{
  nsCOMPtr<nsIAtom> nameAtom(NS_Atomize(aName));
  CustomElementDefinition* data = mCustomDefinitions.GetWeak(nameAtom);

  if (!data) {
    aRetVal.setUndefined();
    return;
  }

  aRetVal.setObject(*data->mConstructor->Callback(aCx));
}

already_AddRefed<Promise>
CustomElementRegistry::WhenDefined(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  RefPtr<Promise> promise = Promise::Create(global, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIAtom> nameAtom(NS_Atomize(aName));
  if (!nsContentUtils::IsCustomElementName(nameAtom)) {
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
    entry.OrInsert([&promise](){ return promise; });
  }

  return promise.forget();
}

namespace {

static void
DoUpgrade(Element* aElement,
          CustomElementConstructor* aConstructor,
          ErrorResult& aRv)
{
  // Rethrow the exception since it might actually throw the exception from the
  // upgrade steps back out to the caller of document.createElement.
  RefPtr<Element> constructResult =
    aConstructor->Construct("Custom Element Upgrade", aRv);
  if (aRv.Failed()) {
    return;
  }

  if (constructResult.get() != aElement) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
}

} // anonymous namespace

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
/* static */ void
CustomElementRegistry::Upgrade(Element* aElement,
                               CustomElementDefinition* aDefinition,
                               ErrorResult& aRv)
{
  aElement->RemoveStates(NS_EVENT_STATE_UNRESOLVED);

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
      nsIAtom* attrName = name->LocalName();

      if (aDefinition->IsInObservedAttributeList(attrName)) {
        int32_t namespaceID = name->NamespaceID();
        nsAutoString attrValue, namespaceURI;
        info.mValue->ToString(attrValue);
        nsContentUtils::NameSpaceManager()->GetNameSpaceURI(namespaceID,
                                                            namespaceURI);

        LifecycleCallbackArgs args = {
          nsDependentAtomString(attrName),
          VoidString(),
          (attrValue.IsEmpty() ? VoidString() : attrValue),
          (namespaceURI.IsEmpty() ? VoidString() : namespaceURI)
        };
        nsContentUtils::EnqueueLifecycleCallback(aElement->OwnerDoc(),
                                                 nsIDocument::eAttributeChanged,
                                                 aElement,
                                                 &args, aDefinition);
      }
    }
  }

  // Step 4.
  // TODO: Bug 1334043 - Implement connected lifecycle callbacks for custom elements

  // Step 5.
  AutoConstructionStackEntry acs(aDefinition->mConstructionStack,
                                 nsGenericHTMLElement::FromContent(aElement));

  // Step 6 and step 7.
  DoUpgrade(aElement, aDefinition->mConstructor, aRv);
  if (aRv.Failed()) {
    data->mState = CustomElementData::State::eFailed;
    // Empty element's custom element reaction queue.
    data->mReactionQueue.Clear();
    return;
  }

  // Step 8.
  data->mState = CustomElementData::State::eCustom;

  // Step 9.
  aElement->SetCustomElementDefinition(aDefinition);

  // This is for old spec.
  nsContentUtils::EnqueueLifecycleCallback(aElement->OwnerDoc(),
                                           nsIDocument::eCreated,
                                           aElement, nullptr, aDefinition);
}

//-----------------------------------------------------
// CustomElementReactionsStack

void
CustomElementReactionsStack::CreateAndPushElementQueue()
{
  // Push a new element queue onto the custom element reactions stack.
  mReactionsStack.AppendElement(MakeUnique<ElementQueue>());
}

void
CustomElementReactionsStack::PopAndInvokeElementQueue()
{
  // Pop the element queue from the custom element reactions stack,
  // and invoke custom element reactions in that queue.
  MOZ_ASSERT(!mReactionsStack.IsEmpty(),
             "Reaction stack shouldn't be empty");

  const uint32_t lastIndex = mReactionsStack.Length() - 1;
  ElementQueue* elementQueue = mReactionsStack.ElementAt(lastIndex).get();
  // Check element queue size in order to reduce function call overhead.
  if (!elementQueue->IsEmpty()) {
    // It is still not clear what error reporting will look like in custom
    // element, see https://github.com/w3c/webcomponents/issues/635.
    // We usually report the error to entry global in gecko, so just follow the
    // same behavior here.
    nsIGlobalObject* global = GetEntryGlobal();
    MOZ_ASSERT(global, "Should always have a entry global here!");
    InvokeReactions(elementQueue, global);
  }

  // InvokeReactions() might create other custom element reactions, but those
  // new reactions should be already consumed and removed at this point.
  MOZ_ASSERT(lastIndex == mReactionsStack.Length() - 1,
             "reactions created by InvokeReactions() should be consumed and removed");

  mReactionsStack.RemoveElementAt(lastIndex);
}

void
CustomElementReactionsStack::EnqueueUpgradeReaction(CustomElementRegistry* aRegistry,
                                                    Element* aElement,
                                                    CustomElementDefinition* aDefinition)
{
  Enqueue(aElement, new CustomElementUpgradeReaction(aRegistry, aDefinition));
}

void
CustomElementReactionsStack::EnqueueCallbackReaction(CustomElementRegistry* aRegistry,
                                                     Element* aElement,
                                                     CustomElementDefinition* aDefinition,
                                                     UniquePtr<CustomElementCallback> aCustomElementCallback)
{
  Enqueue(aElement, new CustomElementCallbackReaction(aRegistry, aDefinition,
                                                      Move(aCustomElementCallback)));
}

void
CustomElementReactionsStack::Enqueue(Element* aElement,
                                     CustomElementReaction* aReaction)
{
  RefPtr<CustomElementData> elementData = aElement->GetCustomElementData();
  MOZ_ASSERT(elementData, "CustomElementData should exist");

  // Add element to the current element queue.
  if (!mReactionsStack.IsEmpty()) {
    mReactionsStack.LastElement()->AppendElement(do_GetWeakReference(aElement));
    elementData->mReactionQueue.AppendElement(aReaction);
    return;
  }

  // If the custom element reactions stack is empty, then:
  // Add element to the backup element queue.
  mBackupQueue.AppendElement(do_GetWeakReference(aElement));
  elementData->mReactionQueue.AppendElement(aReaction);

  if (mIsBackupQueueProcessing) {
    return;
  }

  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();
  RefPtr<ProcessBackupQueueRunnable> processBackupQueueRunnable =
    new ProcessBackupQueueRunnable(this);
  context->DispatchToMicroTask(processBackupQueueRunnable.forget());
}

void
CustomElementReactionsStack::InvokeBackupQueue()
{
  // Check backup queue size in order to reduce function call overhead.
  if (!mBackupQueue.IsEmpty()) {
    // Upgrade reactions won't be scheduled in backup queue and the exception of
    // callback reactions will be automatically reported in CallSetup.
    // If the reactions are invoked from backup queue (in microtask check point),
    // we don't need to pass global object for error reporting.
    InvokeReactions(&mBackupQueue, nullptr);
  }
}

void
CustomElementReactionsStack::InvokeReactions(ElementQueue* aElementQueue,
                                             nsIGlobalObject* aGlobal)
{
  // This is used for error reporting.
  Maybe<AutoEntryScript> aes;
  if (aGlobal) {
    aes.emplace(aGlobal, "custom elements reaction invocation");
  }

  // Note: It's possible to re-enter this method.
  for (uint32_t i = 0; i < aElementQueue->Length(); ++i) {
    nsCOMPtr<Element> element = do_QueryReferent(aElementQueue->ElementAt(i));

    if (!element) {
      continue;
    }

    RefPtr<CustomElementData> elementData = element->GetCustomElementData();
    MOZ_ASSERT(elementData, "CustomElementData should exist");

    auto& reactions = elementData->mReactionQueue;
    for (uint32_t j = 0; j < reactions.Length(); ++j) {
      // Transfer the ownership of the entry due to reentrant invocation of
      // this funciton. The entry will be removed when bug 1379573 is landed.
      auto reaction(Move(reactions.ElementAt(j)));
      if (reaction) {
        ErrorResult rv;
        reaction->Invoke(element, rv);
        if (aes) {
          JSContext* cx = aes->cx();
          if (rv.MaybeSetPendingException(cx)) {
            aes->ReportException();
          }
          MOZ_ASSERT(!JS_IsExceptionPending(cx));
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
  tmp->mPrototype = nullptr;
  tmp->mCallbacks = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CustomElementDefinition)
  mozilla::dom::LifecycleCallbacks* callbacks = tmp->mCallbacks.get();

  if (callbacks->mAttributeChangedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
      "mCallbacks->mAttributeChangedCallback");
    cb.NoteXPCOMChild(callbacks->mAttributeChangedCallback.Value());
  }

  if (callbacks->mCreatedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCallbacks->mCreatedCallback");
    cb.NoteXPCOMChild(callbacks->mCreatedCallback.Value());
  }

  if (callbacks->mAttachedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCallbacks->mAttachedCallback");
    cb.NoteXPCOMChild(callbacks->mAttachedCallback.Value());
  }

  if (callbacks->mDetachedCallback.WasPassed()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCallbacks->mDetachedCallback");
    cb.NoteXPCOMChild(callbacks->mDetachedCallback.Value());
  }

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mConstructor");
  cb.NoteXPCOMChild(tmp->mConstructor);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CustomElementDefinition)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPrototype)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(CustomElementDefinition, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(CustomElementDefinition, Release)


CustomElementDefinition::CustomElementDefinition(nsIAtom* aType,
                                                 nsIAtom* aLocalName,
                                                 Function* aConstructor,
                                                 nsCOMArray<nsIAtom>&& aObservedAttributes,
                                                 JSObject* aPrototype,
                                                 LifecycleCallbacks* aCallbacks,
                                                 uint32_t aDocOrder)
  : mType(aType),
    mLocalName(aLocalName),
    mConstructor(new CustomElementConstructor(aConstructor)),
    mObservedAttributes(Move(aObservedAttributes)),
    mPrototype(aPrototype),
    mCallbacks(aCallbacks),
    mDocOrder(aDocOrder)
{
}


//-----------------------------------------------------
// CustomElementUpgradeReaction

/* virtual */ void
CustomElementUpgradeReaction::Invoke(Element* aElement, ErrorResult& aRv)
{
  mRegistry->Upgrade(aElement, mDefinition, aRv);
}

//-----------------------------------------------------
// CustomElementCallbackReaction

/* virtual */ void
CustomElementCallbackReaction::Invoke(Element* aElement, ErrorResult& aRv)
{
  mCustomElementCallback->Call();
}

} // namespace dom
} // namespace mozilla
