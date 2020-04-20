/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContext_h
#define mozilla_dom_BrowsingContext_h

#include "GVAutoplayRequestUtils.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/LocationBase.h"
#include "mozilla/dom/MaybeDiscarded.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/SyncedContext.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsIDocShell.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsILoadInfo.h"
#include "nsILoadContext.h"

class nsDocShellLoadState;
class nsGlobalWindowOuter;
class nsILoadInfo;
class nsIPrincipal;
class nsOuterWindowProxy;
class PickleIterator;

namespace IPC {
class Message;
}  // namespace IPC

namespace mozilla {

class ErrorResult;
class LogModule;

namespace ipc {
class IProtocol;

template <typename T>
struct IPDLParamTraits;
}  // namespace ipc

namespace dom {
class BrowsingContent;
class BrowsingContextGroup;
class CanonicalBrowsingContext;
class ContentParent;
class Element;
template <typename>
struct Nullable;
template <typename T>
class Sequence;
class StructuredCloneHolder;
class WindowContext;
struct WindowPostMessageOptions;
class WindowProxyHolder;

// Fields are, by default, settable by any process and readable by any process.
// Racy sets will be resolved as-if they occurred in the order the parent
// process finds out about them.
// This defines the default do-nothing implementations for DidSet()
// and CanSet() for all the fields. They may be overloaded to provide
// different behavior for a specific field.
// DidSet() is used to run code in any process that sees the
// the value updated (note: even if the value itself didn't change).
// CanSet() is used to verify that the setting is allowed, and will
// assert if it fails in Debug builds.
#define MOZ_EACH_BC_FIELD(FIELD)                                             \
  FIELD(Name, nsString)                                                      \
  FIELD(Closed, bool)                                                        \
  FIELD(IsActive, bool)                                                      \
  FIELD(EmbedderPolicy, nsILoadInfo::CrossOriginEmbedderPolicy)              \
  FIELD(OpenerPolicy, nsILoadInfo::CrossOriginOpenerPolicy)                  \
  /* Current opener for the BrowsingContext. Weak reference */               \
  FIELD(OpenerId, uint64_t)                                                  \
  FIELD(OnePermittedSandboxedNavigatorId, uint64_t)                          \
  /* WindowID of the inner window which embeds this BC */                    \
  FIELD(EmbedderInnerWindowId, uint64_t)                                     \
  FIELD(CurrentInnerWindowId, uint64_t)                                      \
  FIELD(HadOriginalOpener, bool)                                             \
  FIELD(IsPopupSpam, bool)                                                   \
  /* Controls whether the BrowsingContext is currently considered to be      \
   * activated by a gesture */                                               \
  FIELD(UserActivationState, UserActivation::State)                          \
  /* Hold the audio muted state and should be used on top level browsing     \
   * contexts only */                                                        \
  FIELD(Muted, bool)                                                         \
  FIELD(FeaturePolicy, RefPtr<mozilla::dom::FeaturePolicy>)                  \
  /* See nsSandboxFlags.h for the possible flags. */                         \
  FIELD(SandboxFlags, uint32_t)                                              \
  FIELD(HistoryID, nsID)                                                     \
  FIELD(InRDMPane, bool)                                                     \
  FIELD(Loading, bool)                                                       \
  FIELD(AncestorLoading, bool)                                               \
  FIELD(AllowPlugins, bool)                                                  \
  FIELD(AllowContentRetargeting, bool)                                       \
  FIELD(AllowContentRetargetingOnChildren, bool)                             \
  FIELD(ForceEnableTrackingProtection, bool)                                 \
  /* These field are used to store the states of autoplay media request on   \
   * GeckoView only, and it would only be modified on the top level browsing \
   * context. */                                                             \
  FIELD(GVAudibleAutoplayRequestStatus, GVAutoplayRequestStatus)             \
  FIELD(GVInaudibleAutoplayRequestStatus, GVAutoplayRequestStatus)           \
  /* ScreenOrientation-related APIs */                                       \
  FIELD(CurrentOrientationAngle, float)                                      \
  FIELD(CurrentOrientationType, mozilla::dom::OrientationType)               \
  FIELD(UserAgentOverride, nsString)                                         \
  FIELD(EmbedderElementType, Maybe<nsString>)                                \
  FIELD(MessageManagerGroup, nsString)                                       \
  FIELD(MaxTouchPointsOverride, uint8_t)

// BrowsingContext, in this context, is the cross process replicated
// environment in which information about documents is stored. In
// particular the tree structure of nested browsing contexts is
// represented by the tree of BrowsingContexts.
//
// The tree of BrowsingContexts is created in step with its
// corresponding nsDocShell, and when nsDocShells are connected
// through a parent/child relationship, so are BrowsingContexts. The
// major difference is that BrowsingContexts are replicated (synced)
// to the parent process, making it possible to traverse the
// BrowsingContext tree for a tab, in both the parent and the child
// process.
//
// Trees of BrowsingContexts should only ever contain nodes of the
// same BrowsingContext::Type. This is enforced by asserts in the
// BrowsingContext::Create* methods.
class BrowsingContext : public nsILoadContext, public nsWrapperCache {
  MOZ_DECL_SYNCED_CONTEXT(BrowsingContext, MOZ_EACH_BC_FIELD)

 public:
  enum class Type { Chrome, Content };

  using Children = nsTArray<RefPtr<BrowsingContext>>;

  static void Init();
  static LogModule* GetLog();
  static void CleanupContexts(uint64_t aProcessId);

  // Look up a BrowsingContext in the current process by ID.
  static already_AddRefed<BrowsingContext> Get(uint64_t aId);
  static already_AddRefed<BrowsingContext> Get(GlobalObject&, uint64_t aId) {
    return Get(aId);
  }

  static already_AddRefed<BrowsingContext> GetFromWindow(
      WindowProxyHolder& aProxy);
  static already_AddRefed<BrowsingContext> GetFromWindow(
      GlobalObject&, WindowProxyHolder& aProxy) {
    return GetFromWindow(aProxy);
  }

  // Create a brand-new BrowsingContext object.
  static already_AddRefed<BrowsingContext> Create(BrowsingContext* aParent,
                                                  BrowsingContext* aOpener,
                                                  const nsAString& aName,
                                                  Type aType);

  // Same as the above, but does not immediately attach the browsing context.
  // `EnsureAttached()` must be called before the BrowsingContext is used for a
  // DocShell, BrowserParent, or BrowserBridgeChild.
  static already_AddRefed<BrowsingContext> CreateDetached(
      BrowsingContext* aParent, BrowsingContext* aOpener,
      const nsAString& aName, Type aType);

  // Same as Create, but for a BrowsingContext which does not belong to a
  // visible window, and will always be detached by the process that created it.
  // In contrast, any top-level BrowsingContext created in a content process
  // using Create() is assumed to belong to a <browser> element in the parent
  // process, which will be responsible for detaching it.
  static already_AddRefed<BrowsingContext> CreateWindowless(
      BrowsingContext* aParent, BrowsingContext* aOpener,
      const nsAString& aName, Type aType);

  void EnsureAttached();

  bool EverAttached() const { return mEverAttached; }

  // Cast this object to a canonical browsing context, and return it.
  CanonicalBrowsingContext* Canonical();

  // Is the most recent Document in this BrowsingContext loaded within this
  // process? This may be true with a null mDocShell after the Window has been
  // closed.
  bool IsInProcess() const { return mIsInProcess; }

  // Has this BrowsingContext been discarded. A discarded browsing context has
  // been destroyed, and may not be available on the other side of an IPC
  // message.
  bool IsDiscarded() const { return mIsDiscarded; }

  bool Windowless() const { return mWindowless; }
  void SetWindowless();

  // Get the DocShell for this BrowsingContext if it is in-process, or
  // null if it's not.
  nsIDocShell* GetDocShell() const { return mDocShell; }
  void SetDocShell(nsIDocShell* aDocShell);
  void ClearDocShell() { mDocShell = nullptr; }

  // This cleans up remote outer window proxies that might have been left behind
  // when the browsing context went from being remote to local. It does this by
  // turning them into cross-compartment wrappers to aOuter. If there is already
  // a remote proxy in the compartment of aOuter, then aOuter will get swapped
  // to it and the value of aOuter will be set to the object that used to be the
  // remote proxy and is now an OuterWindowProxy.
  void CleanUpDanglingRemoteOuterWindowProxies(
      JSContext* aCx, JS::MutableHandle<JSObject*> aOuter);

  // Get the embedder element for this BrowsingContext if the embedder is
  // in-process, or null if it's not.
  Element* GetEmbedderElement() const { return mEmbedderElement; }
  void SetEmbedderElement(Element* aEmbedder);

  // Called after the BrowingContext has been embedded in a FrameLoader. This
  // happens after `SetEmbedderElement` is called on the BrowsingContext and
  // after the BrowsingContext has been set on the FrameLoader.
  void Embed();

  // Get the outer window object for this BrowsingContext if it is in-process
  // and still has a docshell, or null otherwise.
  nsPIDOMWindowOuter* GetDOMWindow() const {
    return mDocShell ? mDocShell->GetWindow() : nullptr;
  }

  // Attach the current BrowsingContext to its parent, in both the child and the
  // parent process. BrowsingContext objects are created attached by default, so
  // this method need only be called when restoring cached BrowsingContext
  // objects.
  void Attach(bool aFromIPC = false);

  // Detach the current BrowsingContext from its parent, in both the
  // child and the parent process.
  void Detach(bool aFromIPC = false);

  // Prepare this BrowsingContext to leave the current process.
  void PrepareForProcessChange();

  // Remove all children from the current BrowsingContext and cache
  // them to allow them to be attached again.
  void CacheChildren(bool aFromIPC = false);

  // Restore cached browsing contexts.
  void RestoreChildren(Children&& aChildren, bool aFromIPC = false);

  // Triggers a load in the process which currently owns this BrowsingContext.
  nsresult LoadURI(nsDocShellLoadState* aLoadState,
                   bool aSetNavigating = false);

  nsresult InternalLoad(nsDocShellLoadState* aLoadState,
                        nsIDocShell** aDocShell, nsIRequest** aRequest);

  // If the load state includes a source BrowsingContext has been passed, check
  // to see if we are sandboxed from it as the result of an iframe or CSP
  // sandbox.
  nsresult CheckSandboxFlags(nsDocShellLoadState* aLoadState);

  void DisplayLoadError(const nsAString& aURI);

  // Determine if the current BrowsingContext was 'cached' by the logic in
  // CacheChildren.
  bool IsCached();

  // Check that this browsing context is targetable for navigations (i.e. that
  // it is neither closed, cached, nor discarded).
  bool IsTargetable();

  const nsString& Name() const { return GetName(); }
  void GetName(nsAString& aName) { aName = GetName(); }
  bool NameEquals(const nsAString& aName) { return GetName().Equals(aName); }

  Type GetType() const { return mType; }
  bool IsContent() const { return mType == Type::Content; }
  bool IsChrome() const { return !IsContent(); }

  bool IsTop() const { return !GetParent(); }

  bool IsTopContent() const { return IsContent() && !GetParent(); }

  bool IsContentSubframe() const { return IsContent() && GetParent(); }
  uint64_t Id() const { return mBrowsingContextId; }

  BrowsingContext* GetParent() const {
    MOZ_ASSERT_IF(mParent, mParent->mType == mType);
    return mParent;
  }
  BrowsingContext* Top();

  already_AddRefed<BrowsingContext> GetOpener() const {
    RefPtr<BrowsingContext> opener(Get(GetOpenerId()));
    if (!mIsDiscarded && opener && !opener->mIsDiscarded) {
      MOZ_DIAGNOSTIC_ASSERT(opener->mType == mType);
      return opener.forget();
    }
    return nullptr;
  }
  void SetOpener(BrowsingContext* aOpener) {
    MOZ_DIAGNOSTIC_ASSERT(!aOpener || aOpener->Group() == Group());
    MOZ_DIAGNOSTIC_ASSERT(!aOpener || aOpener->mType == mType);
    SetOpenerId(aOpener ? aOpener->Id() : 0);
  }

  bool HasOpener() const;

  bool HadOriginalOpener() const { return GetHadOriginalOpener(); }

  // Returns true if the browsing context and top context are same origin
  bool SameOriginWithTop();

  /**
   * When a new browsing context is opened by a sandboxed document, it needs to
   * keep track of the browsing context that opened it, so that it can be
   * navigated by it.  This is the "one permitted sandboxed navigator".
   */
  already_AddRefed<BrowsingContext> GetOnePermittedSandboxedNavigator() const {
    return Get(GetOnePermittedSandboxedNavigatorId());
  }
  void SetOnePermittedSandboxedNavigator(BrowsingContext* aNavigator) {
    if (GetOnePermittedSandboxedNavigatorId()) {
      MOZ_ASSERT(false,
                 "One Permitted Sandboxed Navigator should only be set once.");
    } else {
      SetOnePermittedSandboxedNavigatorId(aNavigator ? aNavigator->Id() : 0);
    }
  }

  uint32_t SandboxFlags() { return GetSandboxFlags(); }

  void GetChildren(Children& aChildren);

  void GetWindowContexts(nsTArray<RefPtr<WindowContext>>& aWindows);

  void RegisterWindowContext(WindowContext* aWindow);
  void UnregisterWindowContext(WindowContext* aWindow);
  WindowContext* GetCurrentWindowContext() const {
    return mCurrentWindowContext;
  }

  BrowsingContextGroup* Group() { return mGroup; }

  // WebIDL bindings for nsILoadContext
  Nullable<WindowProxyHolder> GetAssociatedWindow();
  Nullable<WindowProxyHolder> GetTopWindow();
  Element* GetTopFrameElement();
  bool GetIsContent() { return IsContent(); }
  void SetUsePrivateBrowsing(bool aUsePrivateBrowsing, ErrorResult& aError);
  // Needs a different name to disambiguate from the xpidl method with
  // the same signature but different return value.
  void SetUseTrackingProtectionWebIDL(bool aUseTrackingProtection);
  bool UseTrackingProtectionWebIDL() { return UseTrackingProtection(); }
  void GetOriginAttributes(JSContext* aCx, JS::MutableHandle<JS::Value> aVal,
                           ErrorResult& aError);

  bool InRDMPane() const { return GetInRDMPane(); }

  bool IsLoading();

  // ScreenOrientation related APIs
  void SetCurrentOrientation(OrientationType aType, float aAngle) {
    SetCurrentOrientationType(aType);
    SetCurrentOrientationAngle(aAngle);
  }

  void SetRDMPaneOrientation(OrientationType aType, float aAngle) {
    if (InRDMPane()) {
      SetCurrentOrientation(aType, aAngle);
    }
  }

  void SetRDMPaneMaxTouchPoints(uint8_t aMaxTouchPoints) {
    if (InRDMPane()) {
      SetMaxTouchPointsOverride(aMaxTouchPoints);
    }
  }

  void SetAllowContentRetargeting(bool aAllowContentRetargeting);

  // Using the rules for choosing a browsing context we try to find
  // the browsing context with the given name in the set of
  // transitively reachable browsing contexts. Performs access control
  // checks with regard to this.
  // See
  // https://html.spec.whatwg.org/multipage/browsers.html#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name.
  //
  // BrowsingContext::FindWithName(const nsAString&) is equivalent to
  // calling nsIDocShellTreeItem::FindItemWithName(aName, nullptr,
  // nullptr, false, <return value>).
  BrowsingContext* FindWithName(const nsAString& aName,
                                bool aUseEntryGlobalForAccessCheck = true);

  // Find a browsing context in this context's list of
  // children. Doesn't consider the special names, '_self', '_parent',
  // '_top', or '_blank'. Performs access control checks with regard to
  // 'this'.
  BrowsingContext* FindChildWithName(const nsAString& aName,
                                     BrowsingContext& aRequestingContext);

  // Find a browsing context in the subtree rooted at 'this' Doesn't
  // consider the special names, '_self', '_parent', '_top', or
  // '_blank'. Performs access control checks with regard to
  // 'aRequestingContext'.
  BrowsingContext* FindWithNameInSubtree(const nsAString& aName,
                                         BrowsingContext& aRequestingContext);

  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // This function would be called when its corresponding document is activated
  // by user gesture, and we would set the flag in the top level browsing
  // context.
  void NotifyUserGestureActivation();

  // This function would be called when we want to reset the user gesture
  // activation flag of the top level browsing context.
  void NotifyResetUserGestureActivation();

  // Return true if its corresponding document has been activated by user
  // gesture.
  bool HasBeenUserGestureActivated();

  // Return true if its corresponding document has transient user gesture
  // activation and the transient user gesture activation haven't yet timed
  // out.
  bool HasValidTransientUserGestureActivation();

  // Return true if the corresponding document has valid transient user gesture
  // activation and the transient user gesture activation had been consumed
  // successfully.
  bool ConsumeTransientUserGestureActivation();

  // Return the window proxy object that corresponds to this browsing context.
  inline JSObject* GetWindowProxy() const { return mWindowProxy; }
  inline JSObject* GetUnbarrieredWindowProxy() const {
    return mWindowProxy.unbarrieredGet();
  }

  // Set the window proxy object that corresponds to this browsing context.
  void SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) {
    mWindowProxy = aWindowProxy;
  }

  Nullable<WindowProxyHolder> GetWindow();

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(BrowsingContext)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BrowsingContext)
  NS_DECL_NSILOADCONTEXT

  const Children& GetChildren() { return mChildren; }
  const nsTArray<RefPtr<WindowContext>>& GetWindowContexts() {
    return mWindowContexts;
  }

  // Perform a pre-order walk of this BrowsingContext subtree.
  void PreOrderWalk(const std::function<void(BrowsingContext*)>& aCallback) {
    aCallback(this);
    for (auto& child : GetChildren()) {
      child->PreOrderWalk(aCallback);
    }
  }

  // Perform an post-order walk of this BrowsingContext subtree.
  void PostOrderWalk(const std::function<void(BrowsingContext*)>& aCallback) {
    for (auto& child : GetChildren()) {
      child->PostOrderWalk(aCallback);
    }

    aCallback(this);
  }

  // Window APIs that are cross-origin-accessible (from the HTML spec).
  WindowProxyHolder Window();
  BrowsingContext* Self() { return this; }
  void Location(JSContext* aCx, JS::MutableHandle<JSObject*> aLocation,
                ErrorResult& aError);
  void Close(CallerType aCallerType, ErrorResult& aError);
  bool GetClosed(ErrorResult&) { return GetClosed(); }
  void Focus(CallerType aCallerType, ErrorResult& aError);
  void Blur(ErrorResult& aError);
  WindowProxyHolder GetFrames(ErrorResult& aError);
  int32_t Length() const { return mChildren.Length(); }
  Nullable<WindowProxyHolder> GetTop(ErrorResult& aError);
  void GetOpener(JSContext* aCx, JS::MutableHandle<JS::Value> aOpener,
                 ErrorResult& aError) const;
  Nullable<WindowProxyHolder> GetParent(ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      const Sequence<JSObject*>& aTransfer,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const WindowPostMessageOptions& aOptions,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aError);

  void GetCustomUserAgent(nsString& aUserAgent) {
    aUserAgent = Top()->GetUserAgentOverride();
  }
  void SetCustomUserAgent(const nsAString& aUserAgent);

  JSObject* WrapObject(JSContext* aCx);

  static JSObject* ReadStructuredClone(JSContext* aCx,
                                       JSStructuredCloneReader* aReader,
                                       StructuredCloneHolder* aHolder);
  bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder);

  void StartDelayedAutoplayMediaComponents();

  void ResetGVAutoplayRequestStatus();

  /**
   * Information required to initialize a BrowsingContext in another process.
   * This object may be serialized over IPC.
   */
  struct IPCInitializer {
    uint64_t mId = 0;

    // IDs are used for Parent and Opener to allow for this object to be
    // deserialized before other BrowsingContext in the BrowsingContextGroup
    // have been initialized.
    uint64_t mParentId = 0;
    already_AddRefed<BrowsingContext> GetParent();
    already_AddRefed<BrowsingContext> GetOpener();

    uint64_t GetOpenerId() const { return mozilla::Get<IDX_OpenerId>(mFields); }

    bool mCached = false;
    bool mWindowless = false;
    bool mUseRemoteTabs = false;
    bool mUseRemoteSubframes = false;
    OriginAttributes mOriginAttributes;

    FieldTuple mFields;
  };

  // Create an IPCInitializer object for this BrowsingContext.
  IPCInitializer GetIPCInitializer();

  // Create a BrowsingContext object from over IPC.
  static already_AddRefed<BrowsingContext> CreateFromIPC(
      IPCInitializer&& aInitializer, BrowsingContextGroup* aGroup,
      ContentParent* aOriginProcess);

  // Performs access control to check that 'this' can access 'aTarget'.
  bool CanAccess(BrowsingContext* aTarget, bool aConsiderOpener = true);

  bool IsSandboxedFrom(BrowsingContext* aTarget);

  // The runnable will be called once there is idle time, or the top level
  // page has been loaded or if a timeout has fired.
  // Must be called only on the top level BrowsingContext.
  void AddDeprioritizedLoadRunner(nsIRunnable* aRunner);

  RefPtr<SessionStorageManager> GetSessionStorageManager();

  bool PendingInitialization() const { return mPendingInitialization; };
  void SetPendingInitialization(bool aVal) { mPendingInitialization = aVal; };

  const OriginAttributes& OriginAttributesRef() { return mOriginAttributes; }
  nsresult SetOriginAttributes(const OriginAttributes& aAttrs);

 protected:
  virtual ~BrowsingContext();
  BrowsingContext(BrowsingContext* aParent, BrowsingContextGroup* aGroup,
                  uint64_t aBrowsingContextId, Type aType,
                  FieldTuple&& aFields);

 private:
  // Find the special browsing context if aName is '_self', '_parent',
  // '_top', but not '_blank'. The latter is handled in FindWithName
  BrowsingContext* FindWithSpecialName(const nsAString& aName,
                                       BrowsingContext& aRequestingContext);

  // Is it early enough in the BrowsingContext's lifecycle that it is still
  // OK to set OriginAttributes?
  bool CanSetOriginAttributes();

  void AssertOriginAttributesMatchPrivateBrowsing();

  friend class ::nsOuterWindowProxy;
  friend class ::nsGlobalWindowOuter;
  friend class WindowContext;

  // Update the window proxy object that corresponds to this browsing context.
  // This should be called from the window proxy object's objectMoved hook, if
  // the object mWindowProxy points to was moved by the JS GC.
  void UpdateWindowProxy(JSObject* obj, JSObject* old) {
    if (mWindowProxy) {
      MOZ_ASSERT(mWindowProxy == old);
      mWindowProxy = obj;
    }
  }
  // Clear the window proxy object that corresponds to this browsing context.
  // This should be called if the window proxy object is finalized, or it can't
  // reach its browsing context anymore.
  void ClearWindowProxy() { mWindowProxy = nullptr; }

  friend class Location;
  friend class RemoteLocationProxy;
  /**
   * LocationProxy is the class for the native object stored as a private in a
   * RemoteLocationProxy proxy representing a Location object in a different
   * process. It forwards all operations to its BrowsingContext and aggregates
   * its refcount to that BrowsingContext.
   */
  class LocationProxy final : public LocationBase {
   public:
    MozExternalRefCountType AddRef() { return GetBrowsingContext()->AddRef(); }
    MozExternalRefCountType Release() {
      return GetBrowsingContext()->Release();
    }

   protected:
    friend class RemoteLocationProxy;
    BrowsingContext* GetBrowsingContext() override {
      return reinterpret_cast<BrowsingContext*>(
          uintptr_t(this) - offsetof(BrowsingContext, mLocation));
    }

    already_AddRefed<nsIDocShell> GetDocShell() override { return nullptr; }
  };

  // Send a given `BaseTransaction` object to the correct remote.
  void SendCommitTransaction(ContentParent* aParent,
                             const BaseTransaction& aTxn, uint64_t aEpoch);
  void SendCommitTransaction(ContentChild* aChild, const BaseTransaction& aTxn,
                             uint64_t aEpoch);

  // Ensure that opener is in the same BrowsingContextGroup.
  bool CanSet(FieldIndex<IDX_OpenerId>, const uint64_t& aValue,
              ContentParent* aSource) {
    if (aValue != 0) {
      RefPtr<BrowsingContext> opener = Get(aValue);
      return opener && opener->Group() == Group();
    }
    return true;
  }

  void DidSet(FieldIndex<IDX_UserActivationState>);

  // Ensure that we only set the flag on the top level browsingContext.
  // And then, we do a pre-order walk in the tree to refresh the
  // volume of all media elements.
  void DidSet(FieldIndex<IDX_Muted>);

  bool CanSet(FieldIndex<IDX_EmbedderInnerWindowId>, const uint64_t& aValue,
              ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_CurrentInnerWindowId>, const uint64_t& aValue,
              ContentParent* aSource);

  void DidSet(FieldIndex<IDX_CurrentInnerWindowId>);

  bool CanSet(FieldIndex<IDX_IsPopupSpam>, const bool& aValue,
              ContentParent* aSource);

  void DidSet(FieldIndex<IDX_IsPopupSpam>);

  void DidSet(FieldIndex<IDX_GVAudibleAutoplayRequestStatus>);
  void DidSet(FieldIndex<IDX_GVInaudibleAutoplayRequestStatus>);

  void DidSet(FieldIndex<IDX_Loading>);

  void DidSet(FieldIndex<IDX_AncestorLoading>);

  void DidSet(FieldIndex<IDX_UserAgentOverride>);
  bool CanSet(FieldIndex<IDX_UserAgentOverride>, const nsString& aUserAgent,
              ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_EmbedderElementType>,
              const Maybe<nsString>& aInitiatorType, ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_MessageManagerGroup>,
              const nsString& aMessageManagerGroup, ContentParent* aSource);

  bool CanSet(FieldIndex<IDX_AllowContentRetargeting>,
              const bool& aAllowContentRetargeting, ContentParent* aSource);
  bool CanSet(FieldIndex<IDX_AllowContentRetargetingOnChildren>,
              const bool& aAllowContentRetargetingOnChildren,
              ContentParent* aSource);
  bool CanSet(FieldIndex<IDX_AllowPlugins>, const bool& aAllowPlugins,
              ContentParent* aSource);

  template <size_t I, typename T>
  bool CanSet(FieldIndex<I>, const T&, ContentParent*) {
    return true;
  }

  // Overload `DidSet` to get notifications for a particular field being set.
  //
  // You can also overload the variant that gets the old value if you need it.
  template <size_t I>
  void DidSet(FieldIndex<I>) {}
  template <size_t I, typename T>
  void DidSet(FieldIndex<I>, T&& aOldValue) {}

  // True if the process attemping to set field is the same as the owning
  // process.
  bool CheckOnlyOwningProcessCanSet(ContentParent* aSource);

  // True if the process attempting to set field is the same as the embedder's
  // process.
  bool CheckOnlyEmbedderCanSet(ContentParent* aSource);

  // Type of BrowsingContent
  const Type mType;

  // Unique id identifying BrowsingContext
  const uint64_t mBrowsingContextId;

  RefPtr<BrowsingContextGroup> mGroup;
  RefPtr<BrowsingContext> mParent;
  // Note: BrowsingContext_Binding::ClearCachedChildrenValue must be called any
  // time this array is mutated to keep the JS-exposed reflection in sync.
  Children mChildren;
  nsCOMPtr<nsIDocShell> mDocShell;

  RefPtr<Element> mEmbedderElement;

  nsTArray<RefPtr<WindowContext>> mWindowContexts;
  RefPtr<WindowContext> mCurrentWindowContext;

  // This is not a strong reference, but using a JS::Heap for that should be
  // fine. The JSObject stored in here should be a proxy with a
  // nsOuterWindowProxy handler, which will update the pointer from its
  // objectMoved hook and clear it from its finalize hook.
  JS::Heap<JSObject*> mWindowProxy;
  LocationProxy mLocation;

  // OriginAttributes for this BrowsingContext. May not be changed after this
  // BrowsingContext is attached.
  OriginAttributes mOriginAttributes;

  // Determines if private browsing should be used. May not be changed after
  // this BrowsingContext is attached. This field matches mOriginAttributes in
  // content Browsing Contexts. Currently treated as a binary value: 1 - in
  // private mode, 0 - not private mode.
  uint32_t mPrivateBrowsingId;

  // True if Attach() has been called on this BrowsingContext already.
  bool mEverAttached : 1;

  // Is the most recent Document in this BrowsingContext loaded within this
  // process? This may be true with a null mDocShell after the Window has been
  // closed.
  bool mIsInProcess : 1;

  // Has this browsing context been discarded? BrowsingContexts should
  // only be discarded once.
  bool mIsDiscarded : 1;

  // True if this BrowsingContext has no associated visible window, and is owned
  // by whichever process created it, even if top-level.
  bool mWindowless : 1;

  // This is true if the BrowsingContext was out of process, but is now in
  // process, and might have remote window proxies that need to be cleaned up.
  bool mDanglingRemoteOuterProxies : 1;

  // If true, the docShell has not been fully initialized, and may not be used
  // as the target of a load.
  bool mPendingInitialization : 1;

  // True if this BrowsingContext has been embedded in a element in this
  // process.
  bool mEmbeddedByThisProcess : 1;

  // Determines if remote (out-of-process) tabs should be used. May not be
  // changed after this BrowsingContext is attached.
  bool mUseRemoteTabs : 1;

  // Determines if out-of-process iframes should be used. May not be changed
  // after this BrowsingContext is attached.
  bool mUseRemoteSubframes : 1;

  // The start time of user gesture, this is only available if the browsing
  // context is in process.
  TimeStamp mUserGestureStart;

  class DeprioritizedLoadRunner
      : public mozilla::Runnable,
        public mozilla::LinkedListElement<DeprioritizedLoadRunner> {
   public:
    explicit DeprioritizedLoadRunner(nsIRunnable* aInner)
        : Runnable("DeprioritizedLoadRunner"), mInner(aInner) {}

    NS_IMETHOD Run() override {
      if (mInner) {
        RefPtr<nsIRunnable> inner = std::move(mInner);
        inner->Run();
      }

      return NS_OK;
    }

   private:
    RefPtr<nsIRunnable> mInner;
  };

  mozilla::LinkedList<DeprioritizedLoadRunner> mDeprioritizedLoadRunner;

  RefPtr<dom::SessionStorageManager> mSessionStorageManager;
};

/**
 * Gets a WindowProxy object for a BrowsingContext that lives in a different
 * process (creating the object if it doesn't already exist). The WindowProxy
 * object will be in the compartment that aCx is currently in. This should only
 * be called if aContext doesn't hold a docshell, otherwise the BrowsingContext
 * lives in this process, and a same-process WindowProxy should be used (see
 * nsGlobalWindowOuter). This should only be called by bindings code, ToJSValue
 * is the right API to get a WindowProxy for a BrowsingContext.
 *
 * If aTransplantTo is non-null, then the WindowProxy object will eventually be
 * transplanted onto it. Therefore it should be used as the value in the remote
 * proxy map.
 */
extern bool GetRemoteOuterWindowProxy(JSContext* aCx, BrowsingContext* aContext,
                                      JS::Handle<JSObject*> aTransplantTo,
                                      JS::MutableHandle<JSObject*> aRetVal);

using BrowsingContextTransaction = BrowsingContext::BaseTransaction;
using BrowsingContextInitializer = BrowsingContext::IPCInitializer;
using BrowsingContextChildren = BrowsingContext::Children;
using MaybeDiscardedBrowsingContext = MaybeDiscarded<BrowsingContext>;

// Specialize the transaction object for every translation unit it's used in.
extern template class syncedcontext::Transaction<BrowsingContext>;

}  // namespace dom

// Allow sending BrowsingContext objects over IPC.
namespace ipc {
template <>
struct IPDLParamTraits<dom::MaybeDiscarded<dom::BrowsingContext>> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const dom::MaybeDiscarded<dom::BrowsingContext>& aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor,
                   dom::MaybeDiscarded<dom::BrowsingContext>* aResult);
};

template <>
struct IPDLParamTraits<dom::BrowsingContext::IPCInitializer> {
  static void Write(IPC::Message* aMessage, IProtocol* aActor,
                    const dom::BrowsingContext::IPCInitializer& aInitializer);

  static bool Read(const IPC::Message* aMessage, PickleIterator* aIterator,
                   IProtocol* aActor,
                   dom::BrowsingContext::IPCInitializer* aInitializer);
};
}  // namespace ipc
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowsingContext_h)
