/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContext_h
#define mozilla_dom_BrowsingContext_h

#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsILoadInfo.h"

class nsGlobalWindowOuter;
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
class BrowsingContextGroup;
class CanonicalBrowsingContext;
class ContentParent;
template <typename>
struct Nullable;
template <typename T>
class Sequence;
struct WindowPostMessageOptions;
class WindowProxyHolder;

// MOZ_FOR_EACH_SYNCED_FIELD declares BrowsingContext fields that need
// to be synced to the synced versions of BrowsingContext in parent
// and child processes. To add a new field for syncing add a line:
//
// declare(name of new field, storage type, parameter type)
//
// before __VA_ARGS__. This will declare a private member with the
// supplied name prepended with 'm'. If the field needs to be
// initialized in the constructor, then that will have to be done
// manually, and of course keeping the same order as below.
//
// At all times the last line below should be __VA_ARGS__, since that
// acts as a sentinel for callers of MOZ_FOR_EACH_SYNCED_FIELD.

// clang-format off
#define MOZ_FOR_EACH_SYNCED_BC_FIELD(declare, ...)        \
  declare(Name, nsString, nsAString)                   \
  declare(Closed, bool, bool)                          \
  declare(CrossOriginPolicy, nsILoadInfo::CrossOriginPolicy, nsILoadInfo::CrossOriginPolicy) \
  __VA_ARGS__
// clang-format on

#define MOZ_SYNCED_BC_FIELD_NAME(name, ...) m##name
#define MOZ_SYNCED_BC_FIELD_ARGUMENT(name, type, atype) \
  transaction->MOZ_SYNCED_BC_FIELD_NAME(name),
#define MOZ_SYNCED_BC_FIELD_GETTER(name, type, atype) \
  const type& Get##name() const { return MOZ_SYNCED_BC_FIELD_NAME(name); }
#define MOZ_SYNCED_BC_FIELD_SETTER(name, type, atype) \
  void Set##name(const atype& aValue) {               \
    Transaction t;                                    \
    t.MOZ_SYNCED_BC_FIELD_NAME(name).emplace(aValue); \
    t.Commit(this);                                   \
  }
#define MOZ_SYNCED_BC_FIELD_MEMBER(name, type, atype) \
  type MOZ_SYNCED_BC_FIELD_NAME(name);
#define MOZ_SYNCED_BC_FIELD_MAYBE_MEMBER(name, type, atype) \
  mozilla::Maybe<type> MOZ_SYNCED_BC_FIELD_NAME(name);
#define MOZ_SYNCED_BC_FIELD_APPLIER(name, type, atype) \
  if (MOZ_SYNCED_BC_FIELD_NAME(name)) {                \
    aOwner->MOZ_SYNCED_BC_FIELD_NAME(name) =           \
        std::move(*MOZ_SYNCED_BC_FIELD_NAME(name));    \
    MOZ_SYNCED_BC_FIELD_NAME(name).reset();            \
  }

#define MOZ_SYNCED_BC_FIELDS                                              \
 public:                                                                  \
  MOZ_FOR_EACH_SYNCED_BC_FIELD(MOZ_SYNCED_BC_FIELD_GETTER)                \
  MOZ_FOR_EACH_SYNCED_BC_FIELD(MOZ_SYNCED_BC_FIELD_SETTER)                \
  class Transaction {                                                     \
   public:                                                                \
    void Commit(BrowsingContext* aOwner);                                 \
    void Apply(BrowsingContext* aOwner) {                                 \
      MOZ_FOR_EACH_SYNCED_BC_FIELD(MOZ_SYNCED_BC_FIELD_APPLIER)           \
      return; /* without this return clang-format messes up formatting */ \
    }                                                                     \
                                                                          \
    MOZ_FOR_EACH_SYNCED_BC_FIELD(MOZ_SYNCED_BC_FIELD_MAYBE_MEMBER)        \
   private:                                                               \
    friend struct mozilla::ipc::IPDLParamTraits<Transaction>;             \
  };                                                                      \
                                                                          \
 private:                                                                 \
  MOZ_FOR_EACH_SYNCED_BC_FIELD(MOZ_SYNCED_BC_FIELD_MEMBER)

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
class BrowsingContext : public nsWrapperCache,
                        public SupportsWeakPtr<BrowsingContext>,
                        public LinkedListElement<RefPtr<BrowsingContext>> {
  // Do not declare members above MOZ_SYNCED_BC_FIELDS
  MOZ_SYNCED_BC_FIELDS

 public:
  enum class Type { Chrome, Content };

  static void Init();
  static LogModule* GetLog();
  static void CleanupContexts(uint64_t aProcessId);

  // Look up a BrowsingContext in the current process by ID.
  static already_AddRefed<BrowsingContext> Get(uint64_t aId);
  static already_AddRefed<BrowsingContext> Get(GlobalObject&, uint64_t aId) {
    return Get(aId);
  }

  // Create a brand-new BrowsingContext object.
  static already_AddRefed<BrowsingContext> Create(BrowsingContext* aParent,
                                                  BrowsingContext* aOpener,
                                                  const nsAString& aName,
                                                  Type aType);

  // Create a BrowsingContext object from over IPC.
  static already_AddRefed<BrowsingContext> CreateFromIPC(
      BrowsingContext* aParent, BrowsingContext* aOpener,
      const nsAString& aName, uint64_t aId, ContentParent* aOriginProcess);

  // Cast this object to a canonical browsing context, and return it.
  CanonicalBrowsingContext* Canonical();

  // Get the DocShell for this BrowsingContext if it is in-process, or
  // null if it's not.
  nsIDocShell* GetDocShell() { return mDocShell; }
  void SetDocShell(nsIDocShell* aDocShell);

  // Get the outer window object for this BrowsingContext if it is in-process
  // and still has a docshell, or null otherwise.
  nsPIDOMWindowOuter* GetDOMWindow() const {
    return mDocShell ? mDocShell->GetWindow() : nullptr;
  }

  // Attach the current BrowsingContext to its parent, in both the child and the
  // parent process. BrowsingContext objects are created attached by default, so
  // this method need only be called when restoring cached BrowsingContext
  // objects.
  void Attach();

  // Detach the current BrowsingContext from its parent, in both the
  // child and the parent process.
  void Detach();

  // Remove all children from the current BrowsingContext and cache
  // them to allow them to be attached again.
  void CacheChildren();

  // Determine if the current BrowsingContext was 'cached' by the logic in
  // CacheChildren.
  bool IsCached();

  const nsString& Name() const { return mName; }
  void GetName(nsAString& aName) { aName = mName; }
  bool NameEquals(const nsAString& aName) { return mName.Equals(aName); }

  bool IsContent() const { return mType == Type::Content; }

  uint64_t Id() const { return mBrowsingContextId; }

  BrowsingContext* GetParent() { return mParent; }

  void GetChildren(nsTArray<RefPtr<BrowsingContext>>& aChildren);

  BrowsingContext* GetOpener() const { return mOpener; }

  void SetOpener(BrowsingContext* aOpener);

  BrowsingContextGroup* Group() { return mGroup; }

  // Using the rules for choosing a browsing context we try to find
  // the browsing context with the given name in the set of
  // transitively reachable browsing contexts. Performs access control
  // with regards to this.
  // See
  // https://html.spec.whatwg.org/multipage/browsers.html#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name.
  //
  // BrowsingContext::FindWithName(const nsAString&) is equivalent to
  // calling nsIDocShellTreeItem::FindItemWithName(aName, nullptr,
  // nullptr, false, <return value>).
  BrowsingContext* FindWithName(const nsAString& aName);

  // Find a browsing context in this context's list of
  // children. Doesn't consider the special names, '_self', '_parent',
  // '_top', or '_blank'. Performs access control with regard to
  // 'this'.
  BrowsingContext* FindChildWithName(const nsAString& aName);

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

  // These functions would only be called in the top level browsing context.
  // They would set/reset the user gesture activation flag.
  void SetUserGestureActivation();
  void ResetUserGestureActivation();

  // Return true if it corresponding document is activated by user gesture.
  bool GetUserGestureActivation();

  // Return the window proxy object that corresponds to this browsing context.
  inline JSObject* GetWindowProxy() const { return mWindowProxy; }
  // Set the window proxy object that corresponds to this browsing context.
  void SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) {
    mWindowProxy = aWindowProxy;
  }

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(BrowsingContext)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowsingContext)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(BrowsingContext)

  using Children = nsTArray<RefPtr<BrowsingContext>>;
  const Children& GetChildren() { return mChildren; }

  // Window APIs that are cross-origin-accessible (from the HTML spec).
  BrowsingContext* Window() { return Self(); }
  BrowsingContext* Self() { return this; }
  void Location(JSContext* aCx, JS::MutableHandle<JSObject*> aLocation,
                ErrorResult& aError);
  void Close(CallerType aCallerType, ErrorResult& aError);
  bool GetClosed(ErrorResult&) { return mClosed; }
  void Focus(ErrorResult& aError);
  void Blur(ErrorResult& aError);
  BrowsingContext* GetFrames(ErrorResult& aError) { return Self(); }
  int32_t Length() const { return mChildren.Length(); }
  Nullable<WindowProxyHolder> GetTop(ErrorResult& aError);
  void GetOpener(JSContext* aCx, JS::MutableHandle<JS::Value> aOpener,
                 ErrorResult& aError) const;
  Nullable<WindowProxyHolder> GetParent(ErrorResult& aError) const;
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      const Sequence<JSObject*>& aTransfer,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const WindowPostMessageOptions& aOptions,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aError);

  JSObject* WrapObject(JSContext* aCx);

  nsILoadInfo::CrossOriginPolicy CrossOriginPolicy() {
    return mCrossOriginPolicy;
  }

 protected:
  virtual ~BrowsingContext();
  BrowsingContext(BrowsingContext* aParent, BrowsingContext* aOpener,
                  const nsAString& aName, uint64_t aBrowsingContextId,
                  Type aType);

 private:
  // Find the special browsing context if aName is '_self', '_parent',
  // '_top', but not '_blank'. The latter is handled in FindWithName
  BrowsingContext* FindWithSpecialName(const nsAString& aName);

  // Find a browsing context in the subtree rooted at 'this' Doesn't
  // consider the special names, '_self', '_parent', '_top', or
  // '_blank'. Performs access control with regard to
  // 'aRequestingContext'.
  BrowsingContext* FindWithNameInSubtree(const nsAString& aName,
                                         BrowsingContext* aRequestingContext);

  // Performs access control to check that 'this' can access 'aContext'.
  bool CanAccess(BrowsingContext* aContext);

  bool IsActive() const;

  friend class ::nsOuterWindowProxy;
  friend class ::nsGlobalWindowOuter;
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

  BrowsingContext* TopLevelBrowsingContext();

  friend class Location;
  friend class RemoteLocationProxy;
  /**
   * LocationProxy is the class for the native object stored as a private in a
   * RemoteLocationProxy proxy representing a Location object in a different
   * process. It forwards all operations to its BrowsingContext and aggregates
   * its refcount to that BrowsingContext.
   */
  class LocationProxy {
   public:
    MozExternalRefCountType AddRef() { return GetBrowsingContext()->AddRef(); }
    MozExternalRefCountType Release() {
      return GetBrowsingContext()->Release();
    }

    void SetHref(const nsAString& aHref, nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);
    void Replace(const nsAString& aUrl, nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

   private:
    friend class RemoteLocationProxy;
    BrowsingContext* GetBrowsingContext() {
      return reinterpret_cast<BrowsingContext*>(
          uintptr_t(this) - offsetof(BrowsingContext, mLocation));
    }
  };

  // Type of BrowsingContent
  const Type mType;

  // Unique id identifying BrowsingContext
  const uint64_t mBrowsingContextId;

  RefPtr<BrowsingContextGroup> mGroup;
  RefPtr<BrowsingContext> mParent;
  Children mChildren;
  WeakPtr<BrowsingContext> mOpener;
  nsCOMPtr<nsIDocShell> mDocShell;
  // This is not a strong reference, but using a JS::Heap for that should be
  // fine. The JSObject stored in here should be a proxy with a
  // nsOuterWindowProxy handler, which will update the pointer from its
  // objectMoved hook and clear it from its finalize hook.
  JS::Heap<JSObject*> mWindowProxy;
  LocationProxy mLocation;

  // This flag is only valid in the top level browsing context, it indicates
  // whether the corresponding document has been activated by user gesture.
  bool mIsActivatedByUserGesture;
};

/**
 * Gets a WindowProxy object for a BrowsingContext that lives in a different
 * process (creating the object if it doesn't already exist). The WindowProxy
 * object will be in the compartment that aCx is currently in. This should only
 * be called if aContext doesn't hold a docshell, otherwise the BrowsingContext
 * lives in this process, and a same-process WindowProxy should be used (see
 * nsGlobalWindowOuter). This should only be called by bindings code, ToJSValue
 * is the right API to get a WindowProxy for a BrowsingContext.
 */
extern bool GetRemoteOuterWindowProxy(JSContext* aCx, BrowsingContext* aContext,
                                      JS::MutableHandle<JSObject*> aRetVal);

typedef BrowsingContext::Transaction BrowsingContextTransaction;

}  // namespace dom

// Allow sending BrowsingContext objects over IPC.
namespace ipc {
template <>
struct IPDLParamTraits<dom::BrowsingContext> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    dom::BrowsingContext* aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<dom::BrowsingContext>* aResult);
};

template <>
struct IPDLParamTraits<dom::BrowsingContext::Transaction> {
  static void Write(IPC::Message* aMessage, IProtocol* aActor,
                    const dom::BrowsingContext::Transaction& aTransaction);

  static bool Read(const IPC::Message* aMessage, PickleIterator* aIterator,
                   IProtocol* aActor,
                   dom::BrowsingContext::Transaction* aTransaction);
};

}  // namespace ipc
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowsingContext_h)
