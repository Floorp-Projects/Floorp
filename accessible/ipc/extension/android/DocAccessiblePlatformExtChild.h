/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessiblePlatformExtChild_h
#define mozilla_a11y_DocAccessiblePlatformExtChild_h

#include "mozilla/a11y/PDocAccessiblePlatformExtChild.h"

namespace mozilla {
namespace a11y {

class AccessibleWrap;
class DocAccessibleChild;

class DocAccessiblePlatformExtChild : public PDocAccessiblePlatformExtChild {
 public:
  mozilla::ipc::IPCResult RecvPivot(uint64_t aID, int32_t aGranularity,
                                    bool aForward, bool aInclusive);

  mozilla::ipc::IPCResult RecvNavigateText(uint64_t aID, int32_t aGranularity,
                                           int32_t aStartOffset,
                                           int32_t aEndOffset, bool aForward,
                                           bool aSelect);

  mozilla::ipc::IPCResult RecvSetSelection(uint64_t aID, int32_t aStart,
                                           int32_t aEnd);

  mozilla::ipc::IPCResult RecvCut(uint64_t aID);

  mozilla::ipc::IPCResult RecvCopy(uint64_t aID);

  mozilla::ipc::IPCResult RecvPaste(uint64_t aID);

  mozilla::ipc::IPCResult RecvExploreByTouch(uint64_t aID, float aX, float aY);

 private:
  AccessibleWrap* IdToAccessibleWrap(const uint64_t& aID) const;
};
}  // namespace a11y
}  // namespace mozilla

#endif