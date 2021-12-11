/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaIdGenerator_h
#define mozilla_a11y_MsaaIdGenerator_h

#include "mozilla/a11y/IDSet.h"

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/NotNull.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace a11y {

class MsaaAccessible;
class sdnAccessible;

/**
 * This class is responsible for generating child IDs used by our MSAA
 * implementation.
 *
 * If the accessibility cache is disabled, we must differentiate IDs based on
 * the originating process of the accessible, so a portion of the ID's bits are
 * allocated to storing that information. The remaining bits represent the
 * unique ID of the accessible, within that content process.
 *
 * The constants kNumContentProcessIDBits and kNumUniqueIDBits in the
 * implementation are responsible for determining the proportion of bits that
 * are allocated for each purpose.
 *
 * If the accessibility cache is enabled, we don't need to differentiate IDs
 * based on the originating process, so all bits of the ID are used for the
 * unique ID.
 */
class MsaaIdGenerator {
 public:
  uint32_t GetID();
  void ReleaseID(NotNull<MsaaAccessible*> aMsaaAcc);
  void ReleaseID(NotNull<sdnAccessible*> aSdnAcc);
  bool IsChromeID(uint32_t aID);
  bool IsIDForThisContentProcess(uint32_t aID);
  bool IsIDForContentProcess(uint32_t aID,
                             dom::ContentParentId aIPCContentProcessId);
  bool IsSameContentProcessFor(uint32_t aFirstID, uint32_t aSecondID);

  uint32_t GetContentProcessIDFor(dom::ContentParentId aIPCContentProcessID);
  void ReleaseContentProcessIDFor(dom::ContentParentId aIPCContentProcessID);

 private:
  bool ReleaseID(uint32_t aID);
  uint32_t ResolveContentProcessID();

 private:
  UniquePtr<IDSet> mIDSet;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_MsaaIdGenerator_h
