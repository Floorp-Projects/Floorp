/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/DocAccessibleChildBase.h"

namespace mozilla {
namespace a11y {

class LocalAccessible;
class DocAccessiblePlatformExtChild;

/*
 * These objects handle content side communication for an accessible document,
 * and their lifetime is the same as the document they represent.
 */
class DocAccessibleChild : public DocAccessibleChildBase {
  friend DocAccessiblePlatformExtChild;

 public:
  DocAccessibleChild(DocAccessible* aDoc, IProtocol* aManager)
      : DocAccessibleChildBase(aDoc) {
    MOZ_COUNT_CTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
    SetManager(aManager);
  }

  ~DocAccessibleChild() {
    MOZ_COUNT_DTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
  }

  virtual mozilla::ipc::IPCResult RecvRestoreFocus() override;

  virtual mozilla::ipc::IPCResult RecvScrollToPoint(const uint64_t& aID,
                                                    const uint32_t& aScrollType,
                                                    const int32_t& aX,
                                                    const int32_t& aY) override;

  virtual mozilla::ipc::IPCResult RecvAnnounce(
      const uint64_t& aID, const nsAString& aAnnouncement,
      const uint16_t& aPriority) override;

  virtual mozilla::ipc::IPCResult RecvAddToSelection(
      const uint64_t& aID, const int32_t& aStartOffset,
      const int32_t& aEndOffset, bool* aSucceeded) override;

  virtual mozilla::ipc::IPCResult RecvScrollSubstringToPoint(
      const uint64_t& aID, const int32_t& aStartOffset,
      const int32_t& aEndOffset, const uint32_t& aCoordinateType,
      const int32_t& aX, const int32_t& aY) override;

  virtual bool DeallocPDocAccessiblePlatformExtChild(
      PDocAccessiblePlatformExtChild* aActor) override;

  virtual PDocAccessiblePlatformExtChild* AllocPDocAccessiblePlatformExtChild()
      override;

  DocAccessiblePlatformExtChild* GetPlatformExtension();
};

}  // namespace a11y
}  // namespace mozilla

#endif
