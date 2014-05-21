/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * The capabilities of Wifi. These are guaranteed not to change over the
 * lifetime of that particular instance.
 */
enum WifiSecurityMethod {
  "OPEN",
  "WEP",
  "WPA-PSK",
  "WPA-EAP"
};

enum WifiWpaMethod {
  "SIM",
  "PEAP",
  "TTLS",
  "TLS"
};

enum WifiWpaPhase2Method {
  "MSCHAPV2",
  "GTC"
};

enum WifiWpaCertificate {
  "SERVER",
  "USER"
};

[JSImplementation="@mozilla.org/mozwificapabilities;1",
 Func="Navigator::HasWifiManagerSupport"]
interface MozWifiCapabilities {
  [Constant, Cached] readonly attribute sequence<WifiSecurityMethod> security;
  [Constant, Cached] readonly attribute sequence<WifiWpaMethod> eapMethod;
  [Constant, Cached] readonly attribute sequence<WifiWpaPhase2Method> eapPhase2;
  [Constant, Cached] readonly attribute sequence<WifiWpaCertificate> certificate;

  jsonifier;
};

