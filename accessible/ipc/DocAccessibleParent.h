/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleParent_h
#define mozilla_a11y_DocAccessibleParent_h

#include "nsAccessibilityService.h"
#include "mozilla/a11y/PDocAccessibleParent.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace a11y {

class xpcAccessibleGeneric;

/*
 * These objects live in the main process and comunicate with and represent
 * an accessible document in a content process.
 */
class DocAccessibleParent : public ProxyAccessible,
                            public PDocAccessibleParent {
 public:
  DocAccessibleParent()
      : ProxyAccessible(this),
        mParentDoc(kNoParentDoc),
#if defined(XP_WIN)
        mEmulatedWindowHandle(nullptr),
#endif  // defined(XP_WIN)
        mTopLevel(false),
        mShutdown(false) {
    MOZ_COUNT_CTOR_INHERITED(DocAccessibleParent, ProxyAccessible);
    sMaxDocID++;
    mActorID = sMaxDocID;
    MOZ_ASSERT(!LiveDocs().Get(mActorID));
    LiveDocs().Put(mActorID, this);
  }

  ~DocAccessibleParent() {
    LiveDocs().Remove(mActorID);
    MOZ_COUNT_DTOR_INHERITED(DocAccessibleParent, ProxyAccessible);
    MOZ_ASSERT(mChildDocs.Length() == 0);
    MOZ_ASSERT(!ParentDoc());
  }

  void SetTopLevel() { mTopLevel = true; }
  bool IsTopLevel() const { return mTopLevel; }

  bool IsShutdown() const { return mShutdown; }

  /**
   * Mark this actor as shutdown without doing any cleanup.  This should only
   * be called on actors that have just been initialized, so probably only from
   * RecvPDocAccessibleConstructor.
   */
  void MarkAsShutdown() {
    MOZ_ASSERT(mChildDocs.IsEmpty());
    MOZ_ASSERT(mAccessibles.Count() == 0);
    mShutdown = true;
  }

  /*
   * Called when a message from a document in a child process notifies the main
   * process it is firing an event.
   */
  virtual mozilla::ipc::IPCResult RecvEvent(const uint64_t& aID,
                                            const uint32_t& aType) override;

  virtual mozilla::ipc::IPCResult RecvShowEvent(const ShowEventData& aData,
                                                const bool& aFromUser) override;
  virtual mozilla::ipc::IPCResult RecvHideEvent(const uint64_t& aRootID,
                                                const bool& aFromUser) override;
  mozilla::ipc::IPCResult RecvStateChangeEvent(const uint64_t& aID,
                                               const uint64_t& aState,
                                               const bool& aEnabled) final;

  mozilla::ipc::IPCResult RecvCaretMoveEvent(
      const uint64_t& aID,
#if defined(XP_WIN)
      const LayoutDeviceIntRect& aCaretRect,
#endif
      const int32_t& aOffset) final;

  virtual mozilla::ipc::IPCResult RecvTextChangeEvent(
      const uint64_t& aID, const nsString& aStr, const int32_t& aStart,
      const uint32_t& aLen, const bool& aIsInsert,
      const bool& aFromUser) override;

#if defined(XP_WIN)
  virtual mozilla::ipc::IPCResult RecvSyncTextChangeEvent(
      const uint64_t& aID, const nsString& aStr, const int32_t& aStart,
      const uint32_t& aLen, const bool& aIsInsert,
      const bool& aFromUser) override;

  virtual mozilla::ipc::IPCResult RecvFocusEvent(
      const uint64_t& aID, const LayoutDeviceIntRect& aCaretRect) override;
#endif  // defined(XP_WIN)

  virtual mozilla::ipc::IPCResult RecvSelectionEvent(
      const uint64_t& aID, const uint64_t& aWidgetID,
      const uint32_t& aType) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual mozilla::ipc::IPCResult RecvVirtualCursorChangeEvent(
      const uint64_t& aID, const uint64_t& aOldPositionID,
      const int32_t& aOldStartOffset, const int32_t& aOldEndOffset,
      const uint64_t& aNewPositionID, const int32_t& aNewStartOffset,
      const int32_t& aNewEndOffset, const int16_t& aReason,
      const int16_t& aBoundaryType, const bool& aFromUser) override;

  virtual mozilla::ipc::IPCResult RecvScrollingEvent(
      const uint64_t& aID, const uint64_t& aType, const uint32_t& aScrollX,
      const uint32_t& aScrollY, const uint32_t& aMaxScrollX,
      const uint32_t& aMaxScrollY) override;

#if !defined(XP_WIN)
  virtual mozilla::ipc::IPCResult RecvAnnouncementEvent(
      const uint64_t& aID, const nsString& aAnnouncement,
      const uint16_t& aPriority) override;
#endif

  mozilla::ipc::IPCResult RecvRoleChangedEvent(const a11y::role& aRole) final;

  virtual mozilla::ipc::IPCResult RecvBindChildDoc(
      PDocAccessibleParent* aChildDoc, const uint64_t& aID) override;

  void Unbind() {
    if (DocAccessibleParent* parent = ParentDoc()) {
      parent->RemoveChildDoc(this);
    }

    SetParent(nullptr);
  }

  virtual mozilla::ipc::IPCResult RecvShutdown() override;
  void Destroy();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override {
    MOZ_ASSERT(CheckDocTree());
    if (!mShutdown) Destroy();
  }

  /*
   * Return the main processes representation of the parent document (if any)
   * of the document this object represents.
   */
  DocAccessibleParent* ParentDoc() const;
  static const uint64_t kNoParentDoc = UINT64_MAX;

  /*
   * Called when a document in a content process notifies the main process of a
   * new child document.
   */
  ipc::IPCResult AddChildDoc(DocAccessibleParent* aChildDoc, uint64_t aParentID,
                             bool aCreating = true);

  /*
   * Called when the document in the content process this object represents
   * notifies the main process a child document has been removed.
   */
  void RemoveChildDoc(DocAccessibleParent* aChildDoc) {
    ProxyAccessible* parent = aChildDoc->Parent();
    MOZ_ASSERT(parent);
    if (parent) {
      aChildDoc->Parent()->ClearChildDoc(aChildDoc);
    }
    DebugOnly<bool> result = mChildDocs.RemoveElement(aChildDoc->mActorID);
    aChildDoc->mParentDoc = kNoParentDoc;
    MOZ_ASSERT(result);
  }

  void RemoveAccessible(ProxyAccessible* aAccessible) {
    MOZ_DIAGNOSTIC_ASSERT(mAccessibles.GetEntry(aAccessible->ID()));
    mAccessibles.RemoveEntry(aAccessible->ID());
  }

  /**
   * Return the accessible for given id.
   */
  ProxyAccessible* GetAccessible(uintptr_t aID) {
    if (!aID) return this;

    ProxyEntry* e = mAccessibles.GetEntry(aID);
    return e ? e->mProxy : nullptr;
  }

  const ProxyAccessible* GetAccessible(uintptr_t aID) const {
    return const_cast<DocAccessibleParent*>(this)->GetAccessible(aID);
  }

  size_t ChildDocCount() const { return mChildDocs.Length(); }
  const DocAccessibleParent* ChildDocAt(size_t aIdx) const {
    return const_cast<DocAccessibleParent*>(this)->ChildDocAt(aIdx);
  }
  DocAccessibleParent* ChildDocAt(size_t aIdx) {
    return LiveDocs().Get(mChildDocs[aIdx]);
  }

#if defined(XP_WIN)
  void MaybeInitWindowEmulation();
  void SendParentCOMProxy();

  virtual mozilla::ipc::IPCResult RecvGetWindowedPluginIAccessible(
      const WindowsHandle& aHwnd, IAccessibleHolder* aPluginCOMProxy) override;

  /**
   * Set emulated native window handle for a document.
   * @param aWindowHandle emulated native window handle
   */
  void SetEmulatedWindowHandle(HWND aWindowHandle);
  HWND GetEmulatedWindowHandle() const { return mEmulatedWindowHandle; }
#endif

#if !defined(XP_WIN)
  virtual mozilla::ipc::IPCResult RecvBatch(
      const uint64_t& aBatchType, nsTArray<BatchData>&& aData) override;
#endif

 private:
  class ProxyEntry : public PLDHashEntryHdr {
   public:
    explicit ProxyEntry(const void*) : mProxy(nullptr) {}
    ProxyEntry(ProxyEntry&& aOther) : mProxy(aOther.mProxy) {
      aOther.mProxy = nullptr;
    }
    ~ProxyEntry() { delete mProxy; }

    typedef uint64_t KeyType;
    typedef const void* KeyTypePointer;

    bool KeyEquals(const void* aKey) const {
      return mProxy->ID() == (uint64_t)aKey;
    }

    static const void* KeyToPointer(uint64_t aKey) { return (void*)aKey; }

    static PLDHashNumber HashKey(const void* aKey) { return (uint64_t)aKey; }

    enum { ALLOW_MEMMOVE = true };

    ProxyAccessible* mProxy;
  };

  uint32_t AddSubtree(ProxyAccessible* aParent,
                      const nsTArray<AccessibleData>& aNewTree, uint32_t aIdx,
                      uint32_t aIdxInParent);
  MOZ_MUST_USE bool CheckDocTree() const;
  xpcAccessibleGeneric* GetXPCAccessible(ProxyAccessible* aProxy);

  nsTArray<uint64_t> mChildDocs;
  uint64_t mParentDoc;

#if defined(XP_WIN)
  // The handle associated with the emulated window that contains this document
  HWND mEmulatedWindowHandle;

#  if defined(MOZ_SANDBOX)
  mscom::PreservedStreamPtr mParentProxyStream;
#  endif  // defined(MOZ_SANDBOX)
#endif    // defined(XP_WIN)

  /*
   * Conceptually this is a map from IDs to proxies, but we store the ID in the
   * proxy object so we can't use a real map.
   */
  nsTHashtable<ProxyEntry> mAccessibles;
  uint64_t mActorID;
  bool mTopLevel;
  bool mShutdown;

  static uint64_t sMaxDocID;
  static nsDataHashtable<nsUint64HashKey, DocAccessibleParent*>& LiveDocs() {
    static nsDataHashtable<nsUint64HashKey, DocAccessibleParent*> sLiveDocs;
    return sLiveDocs;
  }
};

}  // namespace a11y
}  // namespace mozilla

#endif
