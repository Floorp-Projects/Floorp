/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaIdGenerator_h
#define mozilla_a11y_MsaaIdGenerator_h

#include "mozilla/a11y/IDSet.h"

#include "mozilla/NotNull.h"
#include "nsITimer.h"

namespace mozilla {
namespace a11y {

class MsaaAccessible;
class sdnAccessible;

/**
 * This class is responsible for generating child IDs used by our MSAA
 * implementation.
 */
class MsaaIdGenerator {
 public:
  uint32_t GetID();
  void ReleaseID(NotNull<MsaaAccessible*> aMsaaAcc);
  void ReleaseID(NotNull<sdnAccessible*> aSdnAcc);

 private:
  bool ReleaseID(uint32_t aID);
  void ReleasePendingIDs();

 private:
  static constexpr uint32_t kNumFullIDBits = 31UL;
  IDSet mIDSet{kNumFullIDBits};
  nsTArray<uint32_t> mIDsToRelease;
  nsCOMPtr<nsITimer> mReleaseIDTimer;
  // Whether GetID has been called yet this session.
  bool mGetIDCalled = false;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_MsaaIdGenerator_h
