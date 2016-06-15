/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleParent_h
#define mozilla_a11y_DocAccessibleParent_h

#include "nsAccessibilityService.h"
#include "ProxyAccessible.h"
#include "mozilla/a11y/PDocAccessibleParent.h"
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
    public PDocAccessibleParent
{
public:
  DocAccessibleParent() :
    ProxyAccessible(this), mParentDoc(nullptr),
    mTopLevel(false), mShutdown(false)
  { MOZ_COUNT_CTOR_INHERITED(DocAccessibleParent, ProxyAccessible); }
  ~DocAccessibleParent()
  {
    MOZ_COUNT_DTOR_INHERITED(DocAccessibleParent, ProxyAccessible);
    MOZ_ASSERT(mChildDocs.Length() == 0);
    MOZ_ASSERT(!ParentDoc());
  }

  void SetTopLevel() { mTopLevel = true; }
  bool IsTopLevel() const { return mTopLevel; }

  bool IsShutdown() const { return mShutdown; }

  /*
   * Called when a message from a document in a child process notifies the main
   * process it is firing an event.
   */
  virtual bool RecvEvent(const uint64_t& aID, const uint32_t& aType)
    override;

  virtual bool RecvShowEvent(const ShowEventData& aData, const bool& aFromUser)
    override;
  virtual bool RecvHideEvent(const uint64_t& aRootID, const bool& aFromUser)
    override;
  virtual bool RecvStateChangeEvent(const uint64_t& aID,
                                    const uint64_t& aState,
                                    const bool& aEnabled) override final;

  virtual bool RecvCaretMoveEvent(const uint64_t& aID, const int32_t& aOffset)
    override final;

  virtual bool RecvTextChangeEvent(const uint64_t& aID, const nsString& aStr,
                                   const int32_t& aStart, const uint32_t& aLen,
                                   const bool& aIsInsert,
                                   const bool& aFromUser) override;

  virtual bool RecvSelectionEvent(const uint64_t& aID,
                                  const uint64_t& aWidgetID,
                                  const uint32_t& aType) override;

  virtual bool RecvRoleChangedEvent(const uint32_t& aRole) override final;

  virtual bool RecvBindChildDoc(PDocAccessibleParent* aChildDoc, const uint64_t& aID) override;
  void Unbind()
  {
    mParent = nullptr;
    if (DocAccessibleParent* parent = ParentDoc()) {
      parent->mChildDocs.RemoveElement(this);
    }

    mParentDoc = nullptr;
  }

  virtual bool RecvShutdown() override;
  void Destroy();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override
  {
    MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());
    if (!mShutdown)
      Destroy();
  }

  /*
   * Return the main processes representation of the parent document (if any)
   * of the document this object represents.
   */
  DocAccessibleParent* ParentDoc() const { return mParentDoc; }

  /*
   * Called when a document in a content process notifies the main process of a
   * new child document.
   */
  bool AddChildDoc(DocAccessibleParent* aChildDoc, uint64_t aParentID,
                   bool aCreating = true);

  /*
   * Called when the document in the content process this object represents
   * notifies the main process a child document has been removed.
   */
  void RemoveChildDoc(DocAccessibleParent* aChildDoc)
  {
    aChildDoc->Parent()->SetChildDoc(nullptr);
    mChildDocs.RemoveElement(aChildDoc);
    aChildDoc->mParentDoc = nullptr;
    MOZ_ASSERT(aChildDoc->mChildDocs.Length() == 0);
  }

  void RemoveAccessible(ProxyAccessible* aAccessible)
  {
    MOZ_DIAGNOSTIC_ASSERT(mAccessibles.GetEntry(aAccessible->ID()));
    mAccessibles.RemoveEntry(aAccessible->ID());
  }

  /**
   * Return the accessible for given id.
   */
  ProxyAccessible* GetAccessible(uintptr_t aID)
  {
    if (!aID)
      return this;

    ProxyEntry* e = mAccessibles.GetEntry(aID);
    return e ? e->mProxy : nullptr;
  }

  const ProxyAccessible* GetAccessible(uintptr_t aID) const
    { return const_cast<DocAccessibleParent*>(this)->GetAccessible(aID); }

  size_t ChildDocCount() const { return mChildDocs.Length(); }
  const DocAccessibleParent* ChildDocAt(size_t aIdx) const
    { return mChildDocs[aIdx]; }

private:

  class ProxyEntry : public PLDHashEntryHdr
  {
  public:
    explicit ProxyEntry(const void*) : mProxy(nullptr) {}
    ProxyEntry(ProxyEntry&& aOther) :
      mProxy(aOther.mProxy) { aOther.mProxy = nullptr; }
    ~ProxyEntry() { delete mProxy; }

    typedef uint64_t KeyType;
    typedef const void* KeyTypePointer;

    bool KeyEquals(const void* aKey) const
    { return mProxy->ID() == (uint64_t)aKey; }

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

  nsTArray<DocAccessibleParent*> mChildDocs;
  DocAccessibleParent* mParentDoc;

  /*
   * Conceptually this is a map from IDs to proxies, but we store the ID in the
   * proxy object so we can't use a real map.
   */
  nsTHashtable<ProxyEntry> mAccessibles;
  bool mTopLevel;
  bool mShutdown;
};

}
}

#endif
