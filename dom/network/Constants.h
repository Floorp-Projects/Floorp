/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  static const uint32_t kDefaultType = 5; // ConnectionType::None
  static const bool   kDefaultIsWifi = false;
  static const uint32_t kDefaultDHCPGateway = 0;

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_Constants_h__
