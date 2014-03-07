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

/*
 * These objects live in the main process and comunicate with and represent
 * an accessible document in a content process.
 */
class DocAccessibleParent : public ProxyAccessible,
    public PDocAccessibleParent
{
public:
  DocAccessibleParent() :
    mParentDoc(nullptr)
  { MOZ_COUNT_CTOR_INHERITED(DocAccessibleParent, ProxyAccessible); }
  ~DocAccessibleParent()
  {
    MOZ_COUNT_DTOR_INHERITED(DocAccessibleParent, ProxyAccessible);
    MOZ_ASSERT(mChildDocs.Length() == 0);
    MOZ_ASSERT(!mParentDoc);
  }

  /*
   * Called when a message from a document in a child process notifies the main
   * process it is firing an event.
   */
  virtual bool RecvEvent(const uint32_t& aType) MOZ_OVERRIDE
  {
    return true;
  }

  virtual bool RecvShowEvent(const ShowEventData& aData) MOZ_OVERRIDE;
  virtual bool RecvHideEvent(const uint64_t& aRootID) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mChildDocs.IsEmpty(),
               "why wheren't the child docs destroyed already?");
    mParentDoc ? mParentDoc->RemoveChildDoc(this)
      : GetAccService()->RemoteDocShutdown(this);
  }

  /*
   * Return the main processes representation of the parent document (if any)
   * of the document this object represents.
   */
  DocAccessibleParent* Parent() const { return mParentDoc; }

  /*
   * Called when a document in a content process notifies the main process of a
   * new child document.
   */
  bool AddChildDoc(DocAccessibleParent* aChildDoc, uint64_t aParentID)
  {
    ProxyAccessible* outerDoc = mAccessibles.GetEntry(aParentID)->mProxy;
    if (!outerDoc)
      return false;

    aChildDoc->mParent = outerDoc;
    outerDoc->SetChildDoc(aChildDoc);
    mChildDocs.AppendElement(aChildDoc);
    aChildDoc->mParentDoc = this;
    return true;
  }

  /*
   * Called when the document in the content process this object represents
   * notifies the main process a child document has been removed.
   */
  void RemoveChildDoc(DocAccessibleParent* aChildDoc)
  {
    aChildDoc->mParent->SetChildDoc(nullptr);
    mChildDocs.RemoveElement(aChildDoc);
    aChildDoc->mParentDoc = nullptr;
    MOZ_ASSERT(aChildDoc->mChildDocs.Length() == 0);
  }

  void RemoveAccessible(ProxyAccessible* aAccessible)
  {
    MOZ_ASSERT(mAccessibles.GetEntry(aAccessible->ID()));
    mAccessibles.RemoveEntry(aAccessible->ID());
  }

private:

  class ProxyEntry : public PLDHashEntryHdr
  {
  public:
    ProxyEntry(const void*) : mProxy(nullptr) {}
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

  nsTArray<DocAccessibleParent*> mChildDocs;
  DocAccessibleParent* mParentDoc;

  /*
   * Conceptually this is a map from IDs to proxies, but we store the ID in the
   * proxy object so we can't use a real map.
   */
  nsTHashtable<ProxyEntry> mAccessibles;
};

}
}

#endif
