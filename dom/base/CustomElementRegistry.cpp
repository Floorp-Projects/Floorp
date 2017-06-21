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
#include "nsIParserService.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {

void
CustomElementCallback::Call()
{
  ErrorResult rv;
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
        nsContentUtils::EnqueueLifecycleCallback(
          document, nsIDocument::eAttached, mThisObject);
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
        mArgs.name, mArgs.oldValue, mArgs.newValue, rv);
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
// CustomElementData

CustomElementData::CustomElementData(nsIAtom* aType)
  : CustomElementData(aType, CustomElementData::State::eUndefined)
{
}

CustomElementData::CustomElementData(nsIAtom* aType, State aState)
  : mType(aType)
  , mCurrentCallback(-1)
  , mElementIsBeingCreated(false)
  , mCreatedCallbackInvoked(true)
  , mAssociatedMicroTask(-1)
  , mState(aState)
{
}

void
CustomElementData::RunCallbackQueue()
{
  // Note: It's possible to re-enter this method.
  while (static_cast<uint32_t>(++mCurrentCallback) < mCallbackQueue.Length()) {
    mCallbackQueue[mCurrentCallback]->Call();
  }

  mCallbackQueue.Clear();
  mCurrentCallback = -1;
}

//-----------------------------------------------------
// CustomElementRegistry

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_CLASS(CustomElementRegistry)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CustomElementRegistry)
  tmp->mCustomDefinitions.Clear();
  tmp->mConstructors.clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWhenDefinedPromiseMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CustomElementRegistry)
  for (auto iter = tmp->mCustomDefinitions.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<LifecycleCallbacks>& callbacks = iter.UserData()->mCallbacks;

    if (callbacks->mAttributeChangedCallback.WasPassed()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
        "mCustomDefinitions->mCallbacks->mAttributeChangedCallback");
      cb.NoteXPCOMChild(callbacks->mAttributeChangedCallback.Value());
    }

    if (callbacks->mCreatedCallback.WasPassed()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
        "mCustomDefinitions->mCallbacks->mCreatedCallback");
      cb.NoteXPCOMChild(callbacks->mCreatedCallback.Value());
    }

    if (callbacks->mAttachedCallback.WasPassed()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
        "mCustomDefinitions->mCallbacks->mAttachedCallback");
      cb.NoteXPCOMChild(callbacks->mAttachedCallback.Value());
    }

    if (callbacks->mDetachedCallback.WasPassed()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
        "mCustomDefinitions->mCallbacks->mDetachedCallback");
      cb.NoteXPCOMChild(callbacks->mDetachedCallback.Value());
    }
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWhenDefinedPromiseMap)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CustomElementRegistry)
  for (auto iter = tmp->mCustomDefinitions.Iter(); !iter.Done(); iter.Next()) {
    aCallbacks.Trace(&iter.UserData()->mConstructor,
                     "mCustomDefinitions constructor",
                     aClosure);
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

/* static */ bool
CustomElementRegistry::IsCustomElementEnabled(JSContext* aCx, JSObject* aObject)
{
  return nsContentUtils::IsCustomElementsEnabled() ||
         nsContentUtils::IsWebComponentsEnabled();
}

/* static */ void
CustomElementRegistry::ProcessTopElementQueue()
{
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  nsTArray<RefPtr<CustomElementData>>& stack = *sProcessingStack;
  uint32_t firstQueue = stack.LastIndexOf((CustomElementData*) nullptr);

  for (uint32_t i = firstQueue + 1; i < stack.Length(); ++i) {
    // Callback queue may have already been processed in an earlier
    // element queue or in an element queue that was popped
    // off more recently.
    if (stack[i]->mAssociatedMicroTask != -1) {
      stack[i]->RunCallbackQueue();
      stack[i]->mAssociatedMicroTask = -1;
    }
  }

  // If this was actually the base element queue, don't bother trying to pop
  // the first "queue" marker (sentinel).
  if (firstQueue != 0) {
    stack.SetLength(firstQueue);
  } else {
    // Don't pop sentinel for base element queue.
    stack.SetLength(1);
  }
}

/* static */ void
CustomElementRegistry::XPCOMShutdown()
{
  sProcessingStack.reset();
}

/* static */ Maybe<nsTArray<RefPtr<CustomElementData>>>
CustomElementRegistry::sProcessingStack;

CustomElementRegistry::CustomElementRegistry(nsPIDOMWindowInner* aWindow)
 : mWindow(aWindow)
 , mIsCustomDefinitionRunning(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());
  MOZ_ALWAYS_TRUE(mConstructors.init());

  mozilla::HoldJSObjects(this);

  if (!sProcessingStack) {
    sProcessingStack.emplace();
    // Add the base queue sentinel to the processing stack.
    sProcessingStack->AppendElement((CustomElementData*) nullptr);
  }
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

  CustomElementDefinition* data = mCustomDefinitions.Get(typeAtom);
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

  CustomElementDefinition* definition = mCustomDefinitions.Get(ptr->value());
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

  if (mCustomDefinitions.Get(typeName)) {
    return;
  }

  nsTArray<nsWeakPtr>* unresolved = mCandidatesMap.LookupOrAdd(typeName);
  nsWeakPtr* elem = unresolved->AppendElement();
  *elem = do_GetWeakReference(aElement);
  aElement->AddStates(NS_EVENT_STATE_UNRESOLVED);

  return;
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
  EnqueueLifecycleCallback(nsIDocument::eCreated, aElement, nullptr, definition);
}

void
CustomElementRegistry::EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                                Element* aCustomElement,
                                                LifecycleCallbackArgs* aArgs,
                                                CustomElementDefinition* aDefinition)
{
  RefPtr<CustomElementData> elementData = aCustomElement->GetCustomElementData();
  MOZ_ASSERT(elementData, "CustomElementData should exist");

  // Let DEFINITION be ELEMENT's definition
  CustomElementDefinition* definition = aDefinition;
  if (!definition) {
    mozilla::dom::NodeInfo* info = aCustomElement->NodeInfo();

    // Make sure we get the correct definition in case the element
    // is a extended custom element e.g. <button is="x-button">.
    nsCOMPtr<nsIAtom> typeAtom = elementData ?
      elementData->mType.get() : info->NameAtom();

    definition = mCustomDefinitions.Get(typeAtom);
    if (!definition || definition->mLocalName != info->NameAtom()) {
      // Trying to enqueue a callback for an element that is not
      // a custom element. We are done, nothing to do.
      return;
    }
  }

  // Let CALLBACK be the callback associated with the key NAME in CALLBACKS.
  CallbackFunction* func = nullptr;
  switch (aType) {
    case nsIDocument::eCreated:
      if (definition->mCallbacks->mCreatedCallback.WasPassed()) {
        func = definition->mCallbacks->mCreatedCallback.Value();
      }
      break;

    case nsIDocument::eAttached:
      if (definition->mCallbacks->mAttachedCallback.WasPassed()) {
        func = definition->mCallbacks->mAttachedCallback.Value();
      }
      break;

    case nsIDocument::eDetached:
      if (definition->mCallbacks->mDetachedCallback.WasPassed()) {
        func = definition->mCallbacks->mDetachedCallback.Value();
      }
      break;

    case nsIDocument::eAttributeChanged:
      if (definition->mCallbacks->mAttributeChangedCallback.WasPassed()) {
        func = definition->mCallbacks->mAttributeChangedCallback.Value();
      }
      break;
  }

  // If there is no such callback, stop.
  if (!func) {
    return;
  }

  if (aType == nsIDocument::eCreated) {
    elementData->mCreatedCallbackInvoked = false;
  } else if (!elementData->mCreatedCallbackInvoked) {
    // Callbacks other than created callback must not be enqueued
    // until after the created callback has been invoked.
    return;
  }

  // Add CALLBACK to ELEMENT's callback queue.
  CustomElementCallback* callback = new CustomElementCallback(aCustomElement,
                                                              aType,
                                                              func,
                                                              elementData);
  // Ownership of callback is taken by mCallbackQueue.
  elementData->mCallbackQueue.AppendElement(callback);
  if (aArgs) {
    callback->SetArgs(*aArgs);
  }

  if (!elementData->mElementIsBeingCreated) {
    CustomElementData* lastData =
      sProcessingStack->SafeLastElement(nullptr);

    // A new element queue needs to be pushed if the queue at the
    // top of the stack is associated with another microtask level.
    bool shouldPushElementQueue =
      (!lastData || lastData->mAssociatedMicroTask <
         static_cast<int32_t>(nsContentUtils::MicroTaskLevel()));

    // Push a new element queue onto the processing stack when appropriate
    // (when we enter a new microtask).
    if (shouldPushElementQueue) {
      // Push a sentinel value on the processing stack to mark the
      // boundary between the element queues.
      sProcessingStack->AppendElement((CustomElementData*) nullptr);
    }

    sProcessingStack->AppendElement(elementData);
    elementData->mAssociatedMicroTask =
      static_cast<int32_t>(nsContentUtils::MicroTaskLevel());

    // Add a script runner to pop and process the element queue at
    // the top of the processing stack.
    if (shouldPushElementQueue) {
      // Lifecycle callbacks enqueued by user agent implementation
      // should be invoked prior to returning control back to script.
      // Create a script runner to process the top of the processing
      // stack as soon as it is safe to run script.
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction(&CustomElementRegistry::ProcessTopElementQueue);
      nsContentUtils::AddScriptRunner(runnable);
    }
  }
}

void
CustomElementRegistry::GetCustomPrototype(nsIAtom* aAtom,
                                          JS::MutableHandle<JSObject*> aPrototype)
{
  mozilla::dom::CustomElementDefinition* definition = mCustomDefinitions.Get(aAtom);
  if (definition) {
    aPrototype.set(definition->mPrototype);
  } else {
    aPrototype.set(nullptr);
  }
}

void
CustomElementRegistry::UpgradeCandidates(JSContext* aCx,
                                         nsIAtom* aKey,
                                         CustomElementDefinition* aDefinition,
                                         ErrorResult& aRv)
{
  DocGroup* docGroup = mWindow->GetDocGroup();
  if (!docGroup) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsAutoPtr<nsTArray<nsWeakPtr>> candidates;
  mCandidatesMap.RemoveAndForget(aKey, candidates);
  if (candidates) {


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
  if (mCustomDefinitions.Get(nameAtom)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  /**
   * 4. If this CustomElementRegistry contains an entry with constructor constructor,
   *    then throw a "NotSupportedError" DOMException and abort these steps.
   */
  const auto& ptr = mConstructors.lookup(constructorUnwrapped);
  if (ptr) {
    MOZ_ASSERT(mCustomDefinitions.Get(ptr->value()),
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

    nsIParserService* ps = nsContentUtils::GetParserService();
    if (!ps) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    // bgsound and multicol are unknown html element.
    int32_t tag = ps->HTMLCaseSensitiveAtomTagToId(extendsAtom);
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

      // Note: We call the init from the constructorProtoUnwrapped's compartment
      //       here.
      JS::RootedValue rootedv(cx, JS::ObjectValue(*constructorProtoUnwrapped));
      if (!JS_WrapValue(cx, &rootedv) || !callbacksHolder->Init(cx, rootedv)) {
        aRv.StealExceptionFromJSContext(cx);
        return;
      }
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

  CustomElementDefinition* definition =
    new CustomElementDefinition(nameAtom,
                                localNameAtom,
                                constructor,
                                constructorPrototype,
                                callbacks,
                                0 /* TODO dependent on HTML imports. Bug 877072 */);

  mCustomDefinitions.Put(nameAtom, definition);

  MOZ_ASSERT(mCustomDefinitions.Count() == mConstructors.count(),
             "Number of entries should be the same");

  /**
   * 13. 14. 15. Upgrade candidates
   */
  // TODO: Bug 1299363 - Implement custom element v1 upgrade algorithm
  UpgradeCandidates(cx, nameAtom, definition, aRv);

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
  CustomElementDefinition* data = mCustomDefinitions.Get(nameAtom);

  if (!data) {
    aRetVal.setUndefined();
    return;
  }

  aRetVal.setObject(*data->mConstructor);
  return;
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

  if (mCustomDefinitions.Get(nameAtom)) {
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

void
CustomElementRegistry::Upgrade(Element* aElement,
                               CustomElementDefinition* aDefinition)
{
  // TODO: This function will be replaced to v1 upgrade in bug 1299363
  aElement->RemoveStates(NS_EVENT_STATE_UNRESOLVED);

  // Make sure that the element name matches the name in the definition.
  // (e.g. a definition for x-button extending button should match
  // <button is="x-button"> but not <x-button>.
  if (aElement->NodeInfo()->NameAtom() != aDefinition->mLocalName) {
    //Skip over this element because definition does not apply.
    return;
  }

  MOZ_ASSERT(aElement->IsHTMLElement(aDefinition->mLocalName));

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return;
  }

  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> reflector(cx, aElement->GetWrapper());
  if (reflector) {
    Maybe<JSAutoCompartment> ac;
    JS::Rooted<JSObject*> prototype(cx, aDefinition->mPrototype);
    if (aElement->NodePrincipal()->SubsumesConsideringDomain(nsContentUtils::ObjectPrincipal(prototype))) {
      ac.emplace(cx, reflector);
      if (!JS_WrapObject(cx, &prototype) ||
          !JS_SetPrototype(cx, reflector, prototype)) {
        return;
      }
    } else {
      // We want to set the custom prototype in the compartment where it was
      // registered. We store the prototype from define() without unwrapped,
      // hence the prototype's compartment is the compartment where it was
      // registered.
      // In the case that |reflector| and |prototype| are in different
      // compartments, this will set the prototype on the |reflector|'s wrapper
      // and thus only visible in the wrapper's compartment, since we know
      // reflector's principal does not subsume prototype's in this case.
      ac.emplace(cx, prototype);
      if (!JS_WrapObject(cx, &reflector) ||
          !JS_SetPrototype(cx, reflector, prototype)) {
        return;
      }
    }
  }

  EnqueueLifecycleCallback(nsIDocument::eCreated, aElement, nullptr, aDefinition);
}

//-----------------------------------------------------
// CustomElementReactionsStack

void
CustomElementReactionsStack::CreateAndPushElementQueue()
{
  // Push a new element queue onto the custom element reactions stack.
  mReactionsStack.AppendElement();
}

void
CustomElementReactionsStack::PopAndInvokeElementQueue()
{
  // Pop the element queue from the custom element reactions stack,
  // and invoke custom element reactions in that queue.
  MOZ_ASSERT(!mReactionsStack.IsEmpty(),
             "Reaction stack shouldn't be empty");

  ElementQueue& elementQueue = mReactionsStack.LastElement();
  // Check element queue size in order to reduce function call overhead.
  if (!elementQueue.IsEmpty()) {
    InvokeReactions(elementQueue);
  }

  DebugOnly<bool> isRemovedElement = mReactionsStack.RemoveElement(elementQueue);
  MOZ_ASSERT(isRemovedElement,
             "Reaction stack should have an element queue to remove");
}

void
CustomElementReactionsStack::EnqueueUpgradeReaction(CustomElementRegistry* aRegistry,
                                                    Element* aElement,
                                                    CustomElementDefinition* aDefinition)
{
  Enqueue(aElement, new CustomElementUpgradeReaction(aRegistry, aDefinition));
}

void
CustomElementReactionsStack::Enqueue(Element* aElement,
                                     CustomElementReaction* aReaction)
{
  RefPtr<CustomElementData> elementData = aElement->GetCustomElementData();
  MOZ_ASSERT(elementData, "CustomElementData should exist");

  // Add element to the current element queue.
  if (!mReactionsStack.IsEmpty()) {
    mReactionsStack.LastElement().AppendElement(do_GetWeakReference(aElement));
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
    InvokeReactions(mBackupQueue);
  }
}

void
CustomElementReactionsStack::InvokeReactions(ElementQueue& aElementQueue)
{
  for (uint32_t i = 0; i < aElementQueue.Length(); ++i) {
    nsCOMPtr<Element> element = do_QueryReferent(aElementQueue[i]);

    if (!element) {
      continue;
    }

    RefPtr<CustomElementData> elementData = element->GetCustomElementData();
    MOZ_ASSERT(elementData, "CustomElementData should exist");

    nsTArray<nsAutoPtr<CustomElementReaction>>& reactions =
      elementData->mReactionQueue;
    for (uint32_t j = 0; j < reactions.Length(); ++j) {
      reactions.ElementAt(j)->Invoke(element);
    }
    reactions.Clear();
  }
  aElementQueue.Clear();
}

//-----------------------------------------------------
// CustomElementDefinition

CustomElementDefinition::CustomElementDefinition(nsIAtom* aType,
                                                 nsIAtom* aLocalName,
                                                 JSObject* aConstructor,
                                                 JSObject* aPrototype,
                                                 LifecycleCallbacks* aCallbacks,
                                                 uint32_t aDocOrder)
  : mType(aType),
    mLocalName(aLocalName),
    mConstructor(aConstructor),
    mPrototype(aPrototype),
    mCallbacks(aCallbacks),
    mDocOrder(aDocOrder)
{
}


//-----------------------------------------------------
// CustomElementUpgradeReaction

/* virtual */ void
CustomElementUpgradeReaction::Invoke(Element* aElement)
{
  mRegistry->Upgrade(aElement, mDefinition);
}

} // namespace dom
} // namespace mozilla
