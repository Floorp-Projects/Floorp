/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "mozilla/Hal.h"
#include "ConnectionMainThread.h"

namespace mozilla {
namespace dom {
namespace network {

ConnectionMainThread::ConnectionMainThread(nsPIDOMWindowInner* aWindow)
  : Connection(aWindow)
{
  hal::RegisterNetworkObserver(this);

  hal::NetworkInformation networkInfo;
  hal::GetCurrentNetworkInformation(&networkInfo);

  UpdateFromNetworkInfo(networkInfo, false);
}

ConnectionMainThread::~ConnectionMainThread()
{
  Shutdown();
}

void
ConnectionMainThread::ShutdownInternal()
{
  hal::UnregisterNetworkObserver(this);
}

void
ConnectionMainThread::UpdateFromNetworkInfo(const hal::NetworkInformation& aNetworkInfo,
                                            bool aNotify)
{
  Update(static_cast<ConnectionType>(aNetworkInfo.type()),
         aNetworkInfo.isWifi(), aNetworkInfo.dhcpGateway(), aNotify);
}

void
ConnectionMainThread::Notify(const hal::NetworkInformation& aNetworkInfo)
{
  UpdateFromNetworkInfo(aNetworkInfo, true);
}

} // namespace network
} // namespace dom
} // namespace mozilla
