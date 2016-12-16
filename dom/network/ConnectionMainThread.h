/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_ConnectionMainThread_h
#define mozilla_dom_network_ConnectionMainThread_h

#include "Connection.h"
#include "mozilla/Observer.h"
#include "Types.h"

namespace mozilla {
namespace dom {
namespace network {

class ConnectionMainThread final : public Connection
                                 , public NetworkObserver
{
public:
  explicit ConnectionMainThread(nsPIDOMWindowInner* aWindow);

  // For IObserver
  void Notify(const hal::NetworkInformation& aNetworkInfo) override;

private:
  ~ConnectionMainThread();

  virtual void ShutdownInternal() override;

  /**
   * Update the connection information stored in the object using a
   * NetworkInformation object.
   */
  void UpdateFromNetworkInfo(const hal::NetworkInformation& aNetworkInfo,
                             bool aNotify);
};

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_ConnectionMainThread_h
