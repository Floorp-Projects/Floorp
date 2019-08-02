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
struct WindowPostMessageOptions;
class WindowProxyHolder;

class BrowsingContextBase {
 protected:
  BrowsingContextBase() {
    // default-construct each field.
#define MOZ_BC_FIELD(name, type) m##name = type();
#include "mozilla/dom/BrowsingContextFieldList.h"
  }
  ~BrowsingContextBase() = default;

#define MOZ_BC_FIELD(name, type)                                    \
  type m##name;                                                     \
                                                                    \
  /* shadow to validate fields. aSource is setter process or null*/ \
  void WillSet##name(type const& aValue, ContentParent* aSource) {} \
  void DidSet##name(ContentParent* aSource) {}
#include "mozilla/dom/BrowsingContextFieldList.h"
};

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
class BrowsingContext : public nsWrapperCache, public BrowsingContextBase {
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

  // Get the DocShell for this BrowsingContext if it is in-process, or
  // null if it's not.
  nsIDocShell* GetDocShell() { return mDocShell; }
  void SetDocShell(nsIDocShell* aDocShell);
  void ClearDocShell() { mDocShell = nullptr; }

  // Get the embedder element for this BrowsingContext if the embedder is
  // in-process, or null if it's not.
  Element* GetEmbedderElement() const { return mEmbedderElement; }
  void SetEmbedderElement(Element* aEmbedder);

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

  // Determine if the current BrowsingContext was 'cached' by the logic in
  // CacheChildren.
  bool IsCached();

  // Check that this browsing context is targetable for navigations (i.e. that
  // it is neither closed, cached, nor discarded).
  bool IsTargetable();

  const nsString& Name() const { return mName; }
  void GetName(nsAString& aName) { aName = mName; }
  bool NameEquals(const nsAString& aName) { return mName.Equals(aName); }

  bool IsContent() const { return mType == Type::Content; }
  bool IsChrome() const { return !IsContent(); }

  bool IsTopContent() const { return IsContent() && !GetParent(); }

  uint64_t Id() const { return mBrowsingContextId; }

  BrowsingContext* GetParent() const { return mParent; }

  BrowsingContext* Top();

  already_AddRefed<BrowsingContext> GetOpener() const { return Get(mOpenerId); }
  void SetOpener(BrowsingContext* aOpener) {
    SetOpenerId(aOpener ? aOpener->Id() : 0);
  }

  bool HasOpener() const;

  void GetChildren(Children& aChildren);

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

  const Children& GetChildren() { return mChildren; }

  // Perform a pre-order walk of this BrowsingContext subtree.
  template <typename Func>
  void PreOrderWalk(Func&& aCallback) {
    aCallback(this);
    for (auto& child : GetChildren()) {
      child->PreOrderWalk(aCallback);
    }
  }

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
  Nullable<WindowProxyHolder> GetParent(ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      const Sequence<JSObject*>& aTransfer,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const WindowPostMessageOptions& aOptions,
                      nsIPrincipal& aSubjectPrincipal, ErrorResult& aError);

  JSObject* WrapObject(JSContext* aCx);

  static JSObject* ReadStructuredClone(JSContext* aCx,
                                       JSStructuredCloneReader* aReader,
                                       StructuredCloneHolder* aHolder);
  bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder);

  void StartDelayedAutoplayMediaComponents();

  /**
   * Each synced racy field in a BrowsingContext needs to have a epoch value
   * which is used to resolve race conflicts by ensuring that only the last
   * message received in the parent process wins.
   */
  struct FieldEpochs {
#define MOZ_BC_FIELD(...) /* nothing */
#define MOZ_BC_FIELD_RACY(name, ...) uint64_t m##name = 0;
#include "mozilla/dom/BrowsingContextFieldList.h"
  };

  /**
   * Transaction object. This object is used to specify and then commit
   * modifications to synchronized fields in BrowsingContexts.
   */
  class Transaction {
   public:
    // Apply the changes from this transaction to the specified BrowsingContext
    // in all processes. This method will call the correct `WillSet` and
    // `DidSet` methods, as well as move the value.
    //
    // NOTE: This method mutates `this`, resetting all members to `Nothing()`
    void Commit(BrowsingContext* aOwner);

    // You probably don't want to directly call this method - instead call
    // `Commit`, which will perform the necessary synchronization.
    //
    // |aSource| is the ContentParent which is performing the mutation in the
    // parent process.
    void Apply(BrowsingContext* aOwner, ContentParent* aSource,
               const FieldEpochs* aEpochs = nullptr);

    bool HasNonRacyField() const {
#define MOZ_BC_FIELD(name, ...) \
  if (m##name.isSome()) {       \
    return true;                \
  }
#define MOZ_BC_FIELD_RACY(...) /* nothing */
#include "mozilla/dom/BrowsingContextFieldList.h"

      return false;
    }

#define MOZ_BC_FIELD(name, type) mozilla::Maybe<type> m##name;
#include "mozilla/dom/BrowsingContextFieldList.h"

   private:
    friend struct mozilla::ipc::IPDLParamTraits<Transaction>;
  };

#define MOZ_BC_FIELD(name, type)                        \
  template <typename... Args>                           \
  void Set##name(Args&&... aValue) {                    \
    Transaction txn;                                    \
    txn.m##name.emplace(std::forward<Args>(aValue)...); \
    txn.Commit(this);                                   \
  }                                                     \
                                                        \
  type const& Get##name() const { return m##name; }
#include "mozilla/dom/BrowsingContextFieldList.h"

  /**
   * Information required to initialize a BrowsingContext in another process.
   * This object may be serialized over IPC.
   */
  struct IPCInitializer {
    uint64_t mId;

    // IDs are used for Parent and Opener to allow for this object to be
    // deserialized before other BrowsingContext in the BrowsingContextGroup
    // have been initialized.
    uint64_t mParentId;
    already_AddRefed<BrowsingContext> GetParent();
    already_AddRefed<BrowsingContext> GetOpener();

    bool mCached;
    // Include each field, skipping mOpener, as we want to handle it
    // separately.
#define MOZ_BC_FIELD(name, type) type m##name;
#include "mozilla/dom/BrowsingContextFieldList.h"
  };

  // Create an IPCInitializer object for this BrowsingContext.
  IPCInitializer GetIPCInitializer();

  // Create a BrowsingContext object from over IPC.
  static already_AddRefed<BrowsingContext> CreateFromIPC(
      IPCInitializer&& aInitializer, BrowsingContextGroup* aGroup,
      ContentParent* aOriginProcess);

  // Performs access control to check that 'this' can access 'aTarget'.
  bool CanAccess(BrowsingContext* aTarget, bool aConsiderOpener = true);

 protected:
  virtual ~BrowsingContext();
  BrowsingContext(BrowsingContext* aParent, BrowsingContextGroup* aGroup,
                  uint64_t aBrowsingContextId, Type aType);

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

  // Removes the context from its group and sets mIsDetached to true.
  void Unregister();

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

  // Ensure that opener is in the same BrowsingContextGroup.
  void WillSetOpener(const uint64_t& aValue, ContentParent* aSource) {
    if (aValue != 0) {
      RefPtr<BrowsingContext> opener = Get(aValue);
      MOZ_RELEASE_ASSERT(opener && opener->Group() == Group());
    }
  }

  // Ensure that we only set the flag on the top level browsing context.
  void DidSetIsActivatedByUserGesture(ContentParent* aSource);

  // Type of BrowsingContent
  const Type mType;

  // Unique id identifying BrowsingContext
  const uint64_t mBrowsingContextId;

  RefPtr<BrowsingContextGroup> mGroup;
  RefPtr<BrowsingContext> mParent;
  Children mChildren;
  nsCOMPtr<nsIDocShell> mDocShell;

  RefPtr<Element> mEmbedderElement;

  // This is not a strong reference, but using a JS::Heap for that should be
  // fine. The JSObject stored in here should be a proxy with a
  // nsOuterWindowProxy handler, which will update the pointer from its
  // objectMoved hook and clear it from its finalize hook.
  JS::Heap<JSObject*> mWindowProxy;
  LocationProxy mLocation;

  FieldEpochs mFieldEpochs;

  // Is the most recent Document in this BrowsingContext loaded within this
  // process? This may be true with a null mDocShell after the Window has been
  // closed.
  bool mIsInProcess : 1;

  // Has this browsing context been discarded? BrowsingContexts should
  // only be discarded once.
  bool mIsDiscarded : 1;
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

typedef BrowsingContext::Transaction BrowsingContextTransaction;
typedef BrowsingContext::FieldEpochs BrowsingContextFieldEpochs;
typedef BrowsingContext::IPCInitializer BrowsingContextInitializer;
typedef BrowsingContext::Children BrowsingContextChildren;

}  // namespace dom

// Allow sending BrowsingContext objects over IPC.
namespace ipc {
template <>
struct IPDLParamTraits<dom::BrowsingContext*> {
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

template <>
struct IPDLParamTraits<dom::BrowsingContext::FieldEpochs> {
  static void Write(IPC::Message* aMessage, IProtocol* aActor,
                    const dom::BrowsingContext::FieldEpochs& aEpochs);

  static bool Read(const IPC::Message* aMessage, PickleIterator* aIterator,
                   IProtocol* aActor,
                   dom::BrowsingContext::FieldEpochs* aEpochs);
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
