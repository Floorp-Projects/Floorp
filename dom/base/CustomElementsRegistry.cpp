/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CustomElementsRegistry.h"

#include "mozilla/dom/CustomElementsRegistryBinding.h"
#include "mozilla/dom/WebComponentsBinding.h"

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

CustomElementData::CustomElementData(nsIAtom* aType)
  : mType(aType),
    mCurrentCallback(-1),
    mElementIsBeingCreated(false),
    mCreatedCallbackInvoked(true),
    mAssociatedMicroTask(-1)
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

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_CLASS(CustomElementsRegistry)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CustomElementsRegistry)
  tmp->mCustomDefinitions.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CustomElementsRegistry)
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CustomElementsRegistry)
  for (auto iter = tmp->mCustomDefinitions.Iter(); !iter.Done(); iter.Next()) {
    aCallbacks.Trace(&iter.UserData()->mPrototype,
                     "mCustomDefinitions prototype",
                     aClosure);
  }
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CustomElementsRegistry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CustomElementsRegistry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CustomElementsRegistry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ bool
CustomElementsRegistry::IsCustomElementsEnabled(JSContext* aCx, JSObject* aObject)
{
  JS::Rooted<JSObject*> obj(aCx, aObject);
  if (Preferences::GetBool("dom.webcomponents.customelements.enabled") ||
      Preferences::GetBool("dom.webcomponents.enabled")) {
    return true;
  }

  return false;
}

/* static */ already_AddRefed<CustomElementsRegistry>
CustomElementsRegistry::Create(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  if (!aWindow->GetDocShell()) {
    return nullptr;
  }

  if (!Preferences::GetBool("dom.webcomponents.enabled") &&
      !Preferences::GetBool("dom.webcomponents.customelement.enabled")) {
    return nullptr;
  }

  RefPtr<CustomElementsRegistry> customElementsRegistry =
    new CustomElementsRegistry(aWindow);
  return customElementsRegistry.forget();
}

/* static */ void
CustomElementsRegistry::ProcessTopElementQueue()
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
CustomElementsRegistry::XPCOMShutdown()
{
  sProcessingStack.reset();
}

/* static */ Maybe<nsTArray<RefPtr<CustomElementData>>>
CustomElementsRegistry::sProcessingStack;

CustomElementsRegistry::CustomElementsRegistry(nsPIDOMWindowInner* aWindow)
 : mWindow(aWindow)
{
  mozilla::HoldJSObjects(this);

  if (!sProcessingStack) {
    sProcessingStack.emplace();
    // Add the base queue sentinel to the processing stack.
    sProcessingStack->AppendElement((CustomElementData*) nullptr);
  }
}

CustomElementsRegistry::~CustomElementsRegistry()
{
  mozilla::DropJSObjects(this);
}

CustomElementDefinition*
CustomElementsRegistry::LookupCustomElementDefinition(const nsAString& aLocalName,
                                                      const nsAString* aIs) const
{
  nsCOMPtr<nsIAtom> localNameAtom = NS_Atomize(aLocalName);
  nsCOMPtr<nsIAtom> typeAtom = aIs ? NS_Atomize(*aIs) : localNameAtom;

  CustomElementHashKey key(kNameSpaceID_XHTML, typeAtom);
  CustomElementDefinition* data = mCustomDefinitions.Get(&key);
  if (data && data->mLocalName == localNameAtom) {
    return data;
  }

  return nullptr;
}

void
CustomElementsRegistry::RegisterUnresolvedElement(Element* aElement, nsIAtom* aTypeName)
{
  mozilla::dom::NodeInfo* info = aElement->NodeInfo();

  // Candidate may be a custom element through extension,
  // in which case the custom element type name will not
  // match the element tag name. e.g. <button is="x-button">.
  nsCOMPtr<nsIAtom> typeName = aTypeName;
  if (!typeName) {
    typeName = info->NameAtom();
  }

  CustomElementHashKey key(info->NamespaceID(), typeName);
  if (mCustomDefinitions.Get(&key)) {
    return;
  }

  nsTArray<nsWeakPtr>* unresolved = mCandidatesMap.Get(&key);
  if (!unresolved) {
    unresolved = new nsTArray<nsWeakPtr>();
    // Ownership of unresolved is taken by customElements.
    mCandidatesMap.Put(&key, unresolved);
  }

  nsWeakPtr* elem = unresolved->AppendElement();
  *elem = do_GetWeakReference(aElement);
  aElement->AddStates(NS_EVENT_STATE_UNRESOLVED);

  return;
}

void
CustomElementsRegistry::SetupCustomElement(Element* aElement,
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

  CustomElementDefinition* data = LookupCustomElementDefinition(
    aElement->NodeInfo()->LocalName(), aTypeExtension);

  if (!data) {
    // The type extension doesn't exist in the registry,
    // thus we don't need to enqueue callback or adjust
    // the "is" attribute, but it is possibly an upgrade candidate.
    RegisterUnresolvedElement(aElement, typeAtom);
    return;
  }

  if (data->mLocalName != tagAtom) {
    // The element doesn't match the local name for the
    // definition, thus the element isn't a custom element
    // and we don't need to do anything more.
    return;
  }

  // Enqueuing the created callback will set the CustomElementData on the
  // element, causing prototype swizzling to occur in Element::WrapObject.
  EnqueueLifecycleCallback(nsIDocument::eCreated, aElement, nullptr, data);
}

void
CustomElementsRegistry::EnqueueLifecycleCallback(nsIDocument::ElementCallbackType aType,
                                                 Element* aCustomElement,
                                                 LifecycleCallbackArgs* aArgs,
                                                 CustomElementDefinition* aDefinition)
{
  CustomElementData* elementData = aCustomElement->GetCustomElementData();

  // Let DEFINITION be ELEMENT's definition
  CustomElementDefinition* definition = aDefinition;
  if (!definition) {
    mozilla::dom::NodeInfo* info = aCustomElement->NodeInfo();

    // Make sure we get the correct definition in case the element
    // is a extended custom element e.g. <button is="x-button">.
    nsCOMPtr<nsIAtom> typeAtom = elementData ?
      elementData->mType.get() : info->NameAtom();

    CustomElementHashKey key(info->NamespaceID(), typeAtom);
    definition = mCustomDefinitions.Get(&key);
    if (!definition || definition->mLocalName != info->NameAtom()) {
      // Trying to enqueue a callback for an element that is not
      // a custom element. We are done, nothing to do.
      return;
    }
  }

  if (!elementData) {
    // Create the custom element data the first time
    // that we try to enqueue a callback.
    elementData = new CustomElementData(definition->mType);
    // aCustomElement takes ownership of elementData
    aCustomElement->SetCustomElementData(elementData);
    MOZ_ASSERT(aType == nsIDocument::eCreated,
               "First callback should be the created callback");
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
        NS_NewRunnableFunction(&CustomElementsRegistry::ProcessTopElementQueue);
      nsContentUtils::AddScriptRunner(runnable);
    }
  }
}

void
CustomElementsRegistry::GetCustomPrototype(nsIAtom* aAtom,
                                           JS::MutableHandle<JSObject*> aPrototype)
{
  mozilla::dom::CustomElementHashKey key(kNameSpaceID_XHTML, aAtom);
  mozilla::dom::CustomElementDefinition* definition = mCustomDefinitions.Get(&key);
  if (definition) {
    aPrototype.set(definition->mPrototype);
  } else {
    aPrototype.set(nullptr);
  }
}

JSObject*
CustomElementsRegistry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CustomElementsRegistryBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports* CustomElementsRegistry::GetParentObject() const
{
  return mWindow;
}

void
CustomElementsRegistry::Define(const nsAString& aName,
                               Function& aFunctionConstructor,
                               const ElementDefinitionOptions& aOptions,
                               ErrorResult& aRv)
{
  // TODO: This function will be implemented in bug 1275835
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
CustomElementsRegistry::Get(JSContext* aCx, const nsAString& aName,
                            JS::MutableHandle<JS::Value> aRetVal,
                            ErrorResult& aRv)
{
  // TODO: This function will be implemented in bug 1275838
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

already_AddRefed<Promise>
CustomElementsRegistry::WhenDefined(const nsAString& name, ErrorResult& aRv)
{
  // TODO: This function will be implemented in bug 1275839
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

CustomElementDefinition::CustomElementDefinition(JSObject* aPrototype,
                                                 nsIAtom* aType,
                                                 nsIAtom* aLocalName,
                                                 LifecycleCallbacks* aCallbacks,
                                                 uint32_t aNamespaceID,
                                                 uint32_t aDocOrder)
  : mPrototype(aPrototype),
    mType(aType),
    mLocalName(aLocalName),
    mCallbacks(aCallbacks),
    mNamespaceID(aNamespaceID),
    mDocOrder(aDocOrder)
{
}

} // namespace dom
} // namespace mozilla