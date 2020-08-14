/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessiblePlatformExtChild_h
#define mozilla_a11y_DocAccessiblePlatformExtChild_h

#include "mozilla/a11y/PDocAccessiblePlatformExtChild.h"

namespace mozilla {
namespace a11y {

class HyperTextAccessibleWrap;
class DocAccessibleChild;

class DocAccessiblePlatformExtChild : public PDocAccessiblePlatformExtChild {
 public:
  mozilla::ipc::IPCResult RecvLeftWordAt(
      const uint64_t& aID, const int32_t& aOffset, uint64_t* aStartContainer,
      int32_t* aStartOffset, uint64_t* aEndContainer, int32_t* aEndOffset);

  mozilla::ipc::IPCResult RecvRightWordAt(
      const uint64_t& aID, const int32_t& aOffset, uint64_t* aStartContainer,
      int32_t* aStartOffset, uint64_t* aEndContainer, int32_t* aEndOffset);

  mozilla::ipc::IPCResult RecvNextClusterAt(const uint64_t& aID,
                                            const int32_t& aOffset,
                                            uint64_t* aNextContainer,
                                            int32_t* aNextOffset);

  mozilla::ipc::IPCResult RecvPreviousClusterAt(const uint64_t& aID,
                                                const int32_t& aOffset,
                                                uint64_t* aPrevContainer,
                                                int32_t* aPrevOffset);

  mozilla::ipc::IPCResult RecvTextForRange(const uint64_t& aID,
                                           const int32_t& aStartOffset,
                                           const uint64_t& aEndContainer,
                                           const int32_t& aEndOffset,
                                           nsString* aText);

 private:
  HyperTextAccessibleWrap* IdToHyperTextAccessibleWrap(
      const uint64_t& aID) const;
};
}  // namespace a11y
}  // namespace mozilla

#endif
