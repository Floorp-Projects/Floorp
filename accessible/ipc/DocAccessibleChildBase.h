/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChildBase_h
#define mozilla_a11y_DocAccessibleChildBase_h

#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/a11y/PDocAccessibleChild.h"
#include "mozilla/Unused.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace a11y {

class LocalAccessible;
class AccShowEvent;

class DocAccessibleChildBase : public PDocAccessibleChild {
 public:
  explicit DocAccessibleChildBase(DocAccessible* aDoc)
      : mDoc(aDoc), mIsRemoteConstructed(false) {
    MOZ_COUNT_CTOR(DocAccessibleChildBase);
  }

  ~DocAccessibleChildBase() {
    // Shutdown() should have been called, but maybe it isn't if the process is
    // killed?
    MOZ_ASSERT(!mDoc);
    if (mDoc) {
      mDoc->SetIPCDoc(nullptr);
    }

    MOZ_COUNT_DTOR(DocAccessibleChildBase);
  }

  virtual void Shutdown() {
    DetachDocument();
    SendShutdown();
  }

  /**
   * Serializes a shown tree and sends it to the chrome process.
   */
  void InsertIntoIpcTree(LocalAccessible* aParent, LocalAccessible* aChild,
                         uint32_t aIdxInParent, bool aSuppressShowEvent);
  void ShowEvent(AccShowEvent* aShowEvent);

  virtual void ActorDestroy(ActorDestroyReason) override {
    if (!mDoc) {
      return;
    }

    mDoc->SetIPCDoc(nullptr);
    mDoc = nullptr;
  }

 protected:
  static void FlattenTree(LocalAccessible* aRoot,
                          nsTArray<LocalAccessible*>& aTree);

  static void SerializeTree(nsTArray<LocalAccessible*>& aTree,
                            nsTArray<AccessibleData>& aData);

  virtual void MaybeSendShowEvent(ShowEventData& aData, bool aFromUser) {
    Unused << SendShowEvent(aData, aFromUser);
  }

  void DetachDocument() {
    if (mDoc) {
      mDoc->SetIPCDoc(nullptr);
      mDoc = nullptr;
    }
  }

  bool IsConstructedInParentProcess() const { return mIsRemoteConstructed; }
  void SetConstructedInParentProcess() { mIsRemoteConstructed = true; }

  DocAccessible* mDoc;
  bool mIsRemoteConstructed;

  friend void DocAccessible::DoInitialUpdate();
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_DocAccessibleChildBase_h
