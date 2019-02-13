/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContext_h
#define mozilla_dom_BrowsingContext_h

#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsGlobalWindowOuter;
class nsOuterWindowProxy;
class PickleIterator;

namespace IPC {
class Message;
}  // namespace IPC

namespace mozilla {

class ErrorResult;
class LogModule;
class OOMReporter;

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

  // TODO(farre): We should sync changes from SetName to the parent
  // process. [Bug 1490303]
  void SetName(const nsAString& aName) { mName = aName; }
  const nsString& Name() const { return mName; }
  bool NameEquals(const nsAString& aName) { return mName.Equals(aName); }

  bool IsContent() const { return mType == Type::Content; }

  uint64_t Id() const { return mBrowsingContextId; }

  BrowsingContext* GetParent() { return mParent; }

  void GetChildren(nsTArray<RefPtr<BrowsingContext>>& aChildren);

  BrowsingContext* GetOpener() const { return mOpener; }

  void SetOpener(BrowsingContext* aOpener);

  BrowsingContextGroup* Group() { return mGroup; }

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
                OOMReporter& aError);
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

  already_AddRefed<BrowsingContext> FindChildWithName(const nsAString& aName);

  JSObject* WrapObject(JSContext* aCx);

 protected:
  virtual ~BrowsingContext();
  BrowsingContext(BrowsingContext* aParent, BrowsingContext* aOpener,
                  const nsAString& aName, uint64_t aBrowsingContextId,
                  Type aType);

 private:
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

  // Type of BrowsingContent
  const Type mType;

  // Unique id identifying BrowsingContext
  const uint64_t mBrowsingContextId;

  RefPtr<BrowsingContextGroup> mGroup;
  RefPtr<BrowsingContext> mParent;
  Children mChildren;
  WeakPtr<BrowsingContext> mOpener;
  nsCOMPtr<nsIDocShell> mDocShell;
  nsString mName;
  // This is not a strong reference, but using a JS::Heap for that should be
  // fine. The JSObject stored in here should be a proxy with a
  // nsOuterWindowProxy handler, which will update the pointer from its
  // objectMoved hook and clear it from its finalize hook.
  JS::Heap<JSObject*> mWindowProxy;
  bool mClosed;

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
}  // namespace ipc
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowsingContext_h)
