/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorService.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/StaticPtr.h"
#include "mozJSComponentLoader.h"
#include "mozilla/extensions/WebExtensionContentScript.h"
#include "mozilla/Logging.h"

namespace mozilla {
namespace dom {
namespace {
StaticRefPtr<JSWindowActorService> gJSWindowActorService;
}

/**
 * Helper for calling a named method on a JS Window Actor object with a single
 * parameter.
 *
 * It will do the following:
 *  1. Enter the actor object's compartment.
 *  2. Convert the given parameter into a JS parameter with ToJSValue.
 *  3. Call the named method, passing the single parameter.
 *  4. Place the return value in aRetVal.
 *
 * If an error occurs during this process, this method clears any pending
 * exceptions, and returns a nsresult.
 */
template <typename T>
nsresult CallJSActorMethod(nsWrapperCache* aActor, const char* aName,
                           T& aNativeArg, JS::MutableHandleValue aRetVal) {
  // FIXME(nika): We should avoid atomizing and interning the |aName| strings
  // every time we do this call. Given the limited set of possible IDs, it would
  // be better to cache the `jsid` values.

  aRetVal.setUndefined();

  // Get the wrapper for our actor. If we don't have a wrapper, the target
  // method won't be defined on it. so there's no reason to continue.
  JS::Rooted<JSObject*> actor(RootingCx(), aActor->GetWrapper());
  if (NS_WARN_IF(!actor)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Enter the realm of our actor object to begin running script.
  AutoEntryScript aes(actor, "CallJSActorMethod");
  JSContext* cx = aes.cx();
  JSAutoRealm ar(cx, actor);

  // Get the method we want to call, and produce NS_ERROR_NOT_IMPLEMENTED if
  // it is not present.
  JS::Rooted<JS::Value> func(cx);
  if (NS_WARN_IF(!JS_GetProperty(cx, actor, aName, &func) ||
                 func.isPrimitive())) {
    JS_ClearPendingException(cx);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Convert the native argument to a JS value.
  JS::Rooted<JS::Value> argv(cx);
  if (NS_WARN_IF(!ToJSValue(cx, aNativeArg, &argv))) {
    JS_ClearPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  // Call our method.
  if (NS_WARN_IF(!JS_CallFunctionValue(cx, actor, func,
                                       JS::HandleValueArray(argv), aRetVal))) {
    JS_ClearPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/**
 * Object corresponding to a single actor protocol. This object acts as an
 * Event listener for the actor which is called for events which would
 * trigger actor creation.
 *
 * This object also can act as a carrier for methods and other state related to
 * a single protocol managed by the JSWindowActorService.
 */
class JSWindowActorProtocol final : public nsIObserver,
                                    public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  static already_AddRefed<JSWindowActorProtocol> FromIPC(
      const JSWindowActorInfo& aInfo);
  JSWindowActorInfo ToIPC();

  static already_AddRefed<JSWindowActorProtocol> FromWebIDLOptions(
      const nsAString& aName, const WindowActorOptions& aOptions,
      ErrorResult& aRv);

  struct Sided {
    nsCString mModuleURI;
  };

  struct ParentSide : public Sided {};

  struct EventDecl {
    nsString mName;
    EventListenerFlags mFlags;
    Optional<bool> mPassive;
  };

  struct ChildSide : public Sided {
    nsTArray<EventDecl> mEvents;
    nsTArray<nsCString> mObservers;
  };

  const ParentSide& Parent() const { return mParent; }
  const ChildSide& Child() const { return mChild; }

  void RegisterListenersFor(EventTarget* aRoot);
  void UnregisterListenersFor(EventTarget* aRoot);
  void AddObservers();
  void RemoveObservers();
  bool Matches(BrowsingContext* aBrowsingContext, nsIURI* aURI,
               const nsString& aRemoteType);

 private:
  explicit JSWindowActorProtocol(const nsAString& aName) : mName(aName) {}
  extensions::MatchPatternSet* GetURIMatcher();
  ~JSWindowActorProtocol() = default;

  nsString mName;
  bool mAllFrames = false;
  bool mIncludeChrome = false;
  nsTArray<nsString> mMatches;
  nsTArray<nsString> mRemoteTypes;

  ParentSide mParent;
  ChildSide mChild;

  RefPtr<extensions::MatchPatternSet> mURIMatcher;
};

NS_IMPL_ISUPPORTS(JSWindowActorProtocol, nsIObserver, nsIDOMEventListener);

/* static */ already_AddRefed<JSWindowActorProtocol>
JSWindowActorProtocol::FromIPC(const JSWindowActorInfo& aInfo) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  RefPtr<JSWindowActorProtocol> proto = new JSWindowActorProtocol(aInfo.name());
  // Content processes cannot load chrome browsing contexts, so this flag is
  // irrelevant and not propagated.
  proto->mIncludeChrome = false;
  proto->mAllFrames = aInfo.allFrames();
  proto->mMatches = aInfo.matches();
  proto->mRemoteTypes = aInfo.remoteTypes();
  proto->mChild.mModuleURI.Assign(aInfo.url());

  proto->mChild.mEvents.SetCapacity(aInfo.events().Length());
  for (auto& ipc : aInfo.events()) {
    auto* event = proto->mChild.mEvents.AppendElement();
    event->mName.Assign(ipc.name());
    event->mFlags.mCapture = ipc.capture();
    event->mFlags.mInSystemGroup = ipc.systemGroup();
    event->mFlags.mAllowUntrustedEvents = ipc.allowUntrusted();
    if (ipc.passive()) {
      event->mPassive.Construct(ipc.passive().value());
    }
  }

  proto->mChild.mObservers = aInfo.observers();
  return proto.forget();
}

JSWindowActorInfo JSWindowActorProtocol::ToIPC() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  JSWindowActorInfo info;
  info.name() = mName;
  info.allFrames() = mAllFrames;
  info.matches() = mMatches;
  info.remoteTypes() = mRemoteTypes;
  info.url() = mChild.mModuleURI;

  info.events().SetCapacity(mChild.mEvents.Length());
  for (auto& event : mChild.mEvents) {
    auto* ipc = info.events().AppendElement();
    ipc->name().Assign(event.mName);
    ipc->capture() = event.mFlags.mCapture;
    ipc->systemGroup() = event.mFlags.mInSystemGroup;
    ipc->allowUntrusted() = event.mFlags.mAllowUntrustedEvents;
    if (event.mPassive.WasPassed()) {
      ipc->passive() = Some(event.mPassive.Value());
    }
  }

  info.observers() = mChild.mObservers;
  return info;
}

already_AddRefed<JSWindowActorProtocol>
JSWindowActorProtocol::FromWebIDLOptions(const nsAString& aName,
                                         const WindowActorOptions& aOptions,
                                         ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  RefPtr<JSWindowActorProtocol> proto = new JSWindowActorProtocol(aName);
  proto->mAllFrames = aOptions.mAllFrames;
  proto->mIncludeChrome = aOptions.mIncludeChrome;

  if (aOptions.mMatches.WasPassed()) {
    MOZ_ASSERT(aOptions.mMatches.Value().Length());
    proto->mMatches = aOptions.mMatches.Value();
  }

  if (aOptions.mRemoteTypes.WasPassed()) {
    MOZ_ASSERT(aOptions.mRemoteTypes.Value().Length());
    proto->mRemoteTypes = aOptions.mRemoteTypes.Value();
  }

  proto->mParent.mModuleURI = aOptions.mParent.mModuleURI;
  proto->mChild.mModuleURI = aOptions.mChild.mModuleURI;

  // For each event declared in the source dictionary, initialize the
  // corresponding envent declaration entry in the JSWindowActorProtocol.
  if (aOptions.mChild.mEvents.WasPassed()) {
    auto& entries = aOptions.mChild.mEvents.Value().Entries();
    proto->mChild.mEvents.SetCapacity(entries.Length());

    for (auto& entry : entries) {
      // We don't support the mOnce field, as it doesn't work well in this
      // environment. For now, throw an error in that case.
      if (entry.mValue.mOnce) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }

      // Add the EventDecl to our list of events.
      EventDecl* evt = proto->mChild.mEvents.AppendElement();
      evt->mName = entry.mKey;
      evt->mFlags.mCapture = entry.mValue.mCapture;
      evt->mFlags.mInSystemGroup = entry.mValue.mMozSystemGroup;
      evt->mFlags.mAllowUntrustedEvents =
          entry.mValue.mWantUntrusted.WasPassed()
              ? entry.mValue.mWantUntrusted.Value()
              : false;
      if (entry.mValue.mPassive.WasPassed()) {
        evt->mPassive.Construct(entry.mValue.mPassive.Value());
      }
    }
  }

  if (aOptions.mChild.mObservers.WasPassed()) {
    proto->mChild.mObservers = aOptions.mChild.mObservers.Value();
  }

  return proto.forget();
}

/**
 * This listener only listens for events for the child side of the protocol.
 * This will work in both content and parent processes.
 */
NS_IMETHODIMP JSWindowActorProtocol::HandleEvent(Event* aEvent) {
  // Determine which inner window we're associated with, and get its
  // WindowGlobalChild actor.
  EventTarget* target = aEvent->GetOriginalTarget();
  if (NS_WARN_IF(!target)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner =
      do_QueryInterface(target->GetOwnerGlobal());
  if (NS_WARN_IF(!inner)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<WindowGlobalChild> wgc = inner->GetWindowGlobalChild();
  if (NS_WARN_IF(!wgc)) {
    return NS_ERROR_FAILURE;
  }

  // Ensure our actor is present.
  ErrorResult error;
  RefPtr<JSWindowActorChild> actor = wgc->GetActor(mName, error);
  if (error.Failed()) {
    nsresult rv = error.StealNSResult();

    // Don't raise an error if creation of our actor was vetoed.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return NS_OK;
    }
    return rv;
  }

  // Call the "handleEvent" method on our actor.
  JS::Rooted<JS::Value> dummy(RootingCx());
  return CallJSActorMethod(actor, "handleEvent", aEvent, &dummy);
}

NS_IMETHODIMP JSWindowActorProtocol::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* aData) {
  nsCOMPtr<nsPIDOMWindowInner> inner = do_QueryInterface(aSubject);
  if (NS_WARN_IF(!inner)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<WindowGlobalChild> wgc = inner->GetWindowGlobalChild();
  if (NS_WARN_IF(!wgc)) {
    return NS_ERROR_FAILURE;
  }

  // Ensure our actor is present.
  ErrorResult error;
  RefPtr<JSWindowActorChild> actor = wgc->GetActor(mName, error);
  if (NS_WARN_IF(error.Failed())) {
    nsresult rv = error.StealNSResult();

    // Don't raise an error if creation of our actor was vetoed.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return NS_OK;
    }
    return rv;
  }

  // Get the wrapper for our actor. If we don't have a wrapper, the target
  // method won't be defined on it. so there's no reason to continue.
  JS::Rooted<JSObject*> obj(RootingCx(), actor->GetWrapper());
  if (NS_WARN_IF(!obj)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Enter the realm of our actor object to begin running script.
  AutoEntryScript aes(obj, "JSWindowActorProtocol::Observe");
  JSContext* cx = aes.cx();
  JSAutoRealm ar(cx, obj);

  JS::AutoValueArray<3> argv(cx);
  if (NS_WARN_IF(
          !ToJSValue(cx, aSubject, argv[0]) ||
          !NonVoidByteStringToJsval(cx, nsDependentCString(aTopic), argv[1]))) {
    JS_ClearPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  // aData is an optional parameter.
  if (aData) {
    if (NS_WARN_IF(!ToJSValue(cx, nsDependentString(aData), argv[2]))) {
      JS_ClearPendingException(cx);
      return NS_ERROR_FAILURE;
    }
  } else {
    argv[2].setNull();
  }

  // Call the "observe" method on our actor.
  JS::Rooted<JS::Value> dummy(cx);
  if (NS_WARN_IF(!JS_CallFunctionName(cx, obj, "observe",
                                      JS::HandleValueArray(argv), &dummy))) {
    JS_ClearPendingException(cx);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void JSWindowActorProtocol::RegisterListenersFor(EventTarget* aRoot) {
  EventListenerManager* elm = aRoot->GetOrCreateListenerManager();

  for (auto& event : mChild.mEvents) {
    elm->AddEventListenerByType(EventListenerHolder(this), event.mName,
                                event.mFlags, event.mPassive);
  }
}

void JSWindowActorProtocol::UnregisterListenersFor(EventTarget* aRoot) {
  EventListenerManager* elm = aRoot->GetOrCreateListenerManager();

  for (auto& event : mChild.mEvents) {
    elm->RemoveEventListenerByType(EventListenerHolder(this), event.mName,
                                   event.mFlags);
  }
}

void JSWindowActorProtocol::AddObservers() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  for (auto& topic : mChild.mObservers) {
    // This makes the observer service hold an owning reference to the
    // JSWindowActorProtocol. The JSWindowActorProtocol objects will be living
    // for the full lifetime of the content process, thus the extra strong
    // referencec doesn't have a negative impact.
    os->AddObserver(this, topic.get(), false);
  }
}

void JSWindowActorProtocol::RemoveObservers() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  for (auto& topic : mChild.mObservers) {
    os->RemoveObserver(this, topic.get());
  }
}

extensions::MatchPatternSet* JSWindowActorProtocol::GetURIMatcher() {
  // If we've already created the pattern set, return it.
  if (mURIMatcher || mMatches.IsEmpty()) {
    return mURIMatcher;
  }

  // Constructing the MatchPatternSet requires a JS environment to be run in.
  // We can construct it here in the JSM scope, as we will be keeping it around.
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  GlobalObject global(jsapi.cx(), xpc::PrivilegedJunkScope());

  nsTArray<OwningStringOrMatchPattern> patterns;
  patterns.SetCapacity(mMatches.Length());
  for (nsString& s : mMatches) {
    auto* entry = patterns.AppendElement();
    entry->SetAsString() = s;
  }

  MatchPatternOptions matchPatternOptions;
  // Make MatchPattern's mSchemes create properly.
  matchPatternOptions.mRestrictSchemes = false;
  mURIMatcher = extensions::MatchPatternSet::Constructor(
      global, patterns, matchPatternOptions, IgnoreErrors());
  return mURIMatcher;
}

bool JSWindowActorProtocol::Matches(BrowsingContext* aBrowsingContext,
                                    nsIURI* aURI, const nsString& aRemoteType) {
  if (!mRemoteTypes.IsEmpty() && !mRemoteTypes.Contains(aRemoteType)) {
    return false;
  }

  if (!mAllFrames && aBrowsingContext->GetParent()) {
    return false;
  }

  if (!mIncludeChrome && !aBrowsingContext->IsContent()) {
    return false;
  }

  if (extensions::MatchPatternSet* uriMatcher = GetURIMatcher()) {
    if (!uriMatcher->Matches(aURI)) {
      return false;
    }
  }

  return true;
}

JSWindowActorService::JSWindowActorService() { MOZ_ASSERT(NS_IsMainThread()); }

JSWindowActorService::~JSWindowActorService() { MOZ_ASSERT(NS_IsMainThread()); }

/* static */
already_AddRefed<JSWindowActorService> JSWindowActorService::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gJSWindowActorService) {
    gJSWindowActorService = new JSWindowActorService();
    ClearOnShutdown(&gJSWindowActorService);
  }

  RefPtr<JSWindowActorService> service = gJSWindowActorService.get();
  return service.forget();
}

void JSWindowActorService::RegisterWindowActor(
    const nsAString& aName, const WindowActorOptions& aOptions,
    ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  auto entry = mDescriptors.LookupForAdd(aName);
  if (entry) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  // Insert a new entry for the protocol.
  RefPtr<JSWindowActorProtocol> proto =
      JSWindowActorProtocol::FromWebIDLOptions(aName, aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  entry.OrInsert([&] { return proto; });

  // Send information about the newly added entry to every existing content
  // process.
  AutoTArray<JSWindowActorInfo, 1> ipcInfos{proto->ToIPC()};
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp->SendInitJSWindowActorInfos(ipcInfos);
  }

  // Register event listeners for any existing window roots.
  for (EventTarget* root : mRoots) {
    proto->RegisterListenersFor(root);
  }

  // Add observers to the protocol.
  proto->AddObservers();
}

void JSWindowActorService::UnregisterWindowActor(const nsAString& aName) {
  nsAutoString name(aName);

  RefPtr<JSWindowActorProtocol> proto;
  if (mDescriptors.Remove(aName, getter_AddRefs(proto))) {
    // If we're in the parent process, also unregister the window actor in all
    // live content processes.
    if (XRE_IsParentProcess()) {
      for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
        Unused << cp->SendUnregisterJSWindowActor(name);
      }
    }

    // Remove listeners for this actor from each of our window roots.
    for (EventTarget* root : mRoots) {
      proto->UnregisterListenersFor(root);
    }

    // Remove observers for this actor from observer serivce.
    proto->RemoveObservers();
  }
}

void JSWindowActorService::LoadJSWindowActorInfos(
    nsTArray<JSWindowActorInfo>& aInfos) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsContentProcess());

  for (uint32_t i = 0, len = aInfos.Length(); i < len; i++) {
    // Create our JSWindowActorProtocol, register it in mDescriptors.
    RefPtr<JSWindowActorProtocol> proto =
        JSWindowActorProtocol::FromIPC(aInfos[i]);
    mDescriptors.Put(aInfos[i].name(), proto);

    // Register listeners for each window root.
    for (EventTarget* root : mRoots) {
      proto->RegisterListenersFor(root);
    }

    // Add observers for each actor.
    proto->AddObservers();
  }
}

void JSWindowActorService::GetJSWindowActorInfos(
    nsTArray<JSWindowActorInfo>& aInfos) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  for (auto iter = mDescriptors.ConstIter(); !iter.Done(); iter.Next()) {
    aInfos.AppendElement(iter.Data()->ToIPC());
  }
}

void JSWindowActorService::ConstructActor(
    const nsAString& aName, bool aParentSide, BrowsingContext* aBrowsingContext,
    nsIURI* aURI, const nsString& aRemoteType, JS::MutableHandleObject aActor,
    ErrorResult& aRv) {
  MOZ_ASSERT_IF(aParentSide, XRE_IsParentProcess());
  MOZ_ASSERT(aBrowsingContext, "DocShell without a BrowsingContext!");
  MOZ_ASSERT(aURI, "Must have URI!");
  // Constructing an actor requires a running script, so push an AutoEntryScript
  // onto the stack.
  AutoEntryScript aes(xpc::PrivilegedJunkScope(), "JSWindowActor construction");
  JSContext* cx = aes.cx();

  // Load our descriptor
  RefPtr<JSWindowActorProtocol> proto = mDescriptors.Get(aName);
  if (!proto) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  const JSWindowActorProtocol::Sided* side;
  if (aParentSide) {
    side = &proto->Parent();
  } else {
    side = &proto->Child();
  }

  // Check if our current BrowsingContext and URI matches the requirements for
  // this actor to load.
  if (!proto->Matches(aBrowsingContext, aURI, aRemoteType)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  // Load the module using mozJSComponentLoader.
  RefPtr<mozJSComponentLoader> loader = mozJSComponentLoader::Get();
  MOZ_ASSERT(loader);

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);
  aRv = loader->Import(cx, side->mModuleURI, &global, &exports);
  if (aRv.Failed()) {
    return;
  }
  MOZ_ASSERT(exports, "null exports!");

  // Load the specific property from our module.
  JS::RootedValue ctor(cx);
  nsAutoString ctorName(aName);
  ctorName.Append(aParentSide ? NS_LITERAL_STRING("Parent")
                              : NS_LITERAL_STRING("Child"));
  if (!JS_GetUCProperty(cx, exports, ctorName.get(), ctorName.Length(),
                        &ctor)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  // Invoke the constructor loaded from the module.
  if (!JS::Construct(cx, ctor, JS::HandleValueArray::empty(), aActor)) {
    aRv.NoteJSContextException(cx);
    return;
  }
}

void JSWindowActorService::ReceiveMessage(nsISupports* aTarget,
                                          JS::RootedObject& aObj,
                                          const nsString& aMessageName,
                                          ipc::StructuredCloneData& aData) {
  IgnoredErrorResult error;
  AutoEntryScript aes(aObj, "WindowGlobalChild Message Handler");
  JSContext* cx = aes.cx();

  // We passed the unwrapped object to AutoEntryScript so we now need to
  // enter the realm of the global object that represents the realm of our
  // callback.
  JSAutoRealm ar(cx, aObj);
  JS::RootedValue json(cx, JS::NullValue());

  // Deserialize our data into a JS object in the correct compartment.
  aData.Read(aes.cx(), &json, error);
  if (NS_WARN_IF(error.Failed())) {
    JS_ClearPendingException(cx);
    return;
  }

  RootedDictionary<ReceiveMessageArgument> argument(cx);
  argument.mObjects = JS_NewPlainObject(cx);
  argument.mTarget = aTarget;
  argument.mName = aMessageName;
  argument.mData = json;
  argument.mJson = json;
  argument.mSync = false;

  JS::RootedValue argv(cx);
  if (NS_WARN_IF(!ToJSValue(cx, argument, &argv))) {
    return;
  }

  // Now that we have finished, call the recvAsyncMessage callback.
  JS::RootedValue dummy(cx);
  if (NS_WARN_IF(!JS_CallFunctionName(cx, aObj, "recvAsyncMessage",
                                      JS::HandleValueArray(argv), &dummy))) {
    JS_ClearPendingException(cx);
    return;
  }
}

void JSWindowActorService::RegisterWindowRoot(EventTarget* aRoot) {
  MOZ_ASSERT(!mRoots.Contains(aRoot));
  mRoots.AppendElement(aRoot);

  // Register event listeners on the newly added Window Root.
  for (auto iter = mDescriptors.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->RegisterListenersFor(aRoot);
  }
}

/* static */ void JSWindowActorService::UnregisterWindowRoot(
    EventTarget* aRoot) {
  if (gJSWindowActorService) {
    // NOTE: No need to unregister listeners here, as the root is going away.
    gJSWindowActorService->mRoots.RemoveElement(aRoot);
  }
}

}  // namespace dom
}  // namespace mozilla
