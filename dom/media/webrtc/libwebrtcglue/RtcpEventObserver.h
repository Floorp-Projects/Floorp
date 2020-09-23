/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RTCP_EVENT_OBSERVER_
#define RTCP_EVENT_OBSERVER_

namespace mozilla {
/**
 * This provides an interface to allow for receiving notifications
 * of rtcp bye packets and timeouts.
 */
class RtcpEventObserver {
 public:
  virtual void OnRtcpBye() = 0;
  virtual void OnRtcpTimeout() = 0;
};

}  // namespace mozilla
#endif
