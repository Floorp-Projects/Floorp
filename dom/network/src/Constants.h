/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_Constants_h__
#define mozilla_dom_network_Constants_h__

/**
 * A set of constants to be used by network backends.
 */
namespace mozilla {
namespace dom {
namespace network {

  static const double kDefaultBandwidth    = -1.0;
  static const bool   kDefaultCanBeMetered = false;
  static const bool   kDefaultIsWifi = false;
  static const uint32_t kDefaultDHCPGateway = 0;

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_Constants_h__
