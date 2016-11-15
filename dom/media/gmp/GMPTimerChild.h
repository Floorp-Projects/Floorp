/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPTimerChild_h_
#define GMPTimerChild_h_

#include "mozilla/gmp/PGMPTimerChild.h"
#include "mozilla/Monitor.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "gmp-errors.h"
#include "gmp-platform.h"

namespace mozilla {
namespace gmp {

class GMPChild;

class GMPTimerChild : public PGMPTimerChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPTimerChild)

  explicit GMPTimerChild(GMPChild* aPlugin);

  GMPErr SetTimer(GMPTask* aTask, int64_t aTimeoutMS);

protected:
  // GMPTimerChild
  mozilla::ipc::IPCResult RecvTimerExpired(const uint32_t& aTimerId) override;

private:
  ~GMPTimerChild();

  nsDataHashtable<nsUint32HashKey, GMPTask*> mTimers;
  uint32_t mTimerCount;

  GMPChild* mPlugin;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPTimerChild_h_
