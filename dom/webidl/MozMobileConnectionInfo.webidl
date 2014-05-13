/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum MobileConnectionState {"notSearching", "searching", "denied", "registered"};
enum MobileConnectionType {"gsm", "gprs", "edge", "umts", "hsdpa", "hsupa",
                           "hspa", "hspa+", "is95a", "is95b", "1xrtt", "evdo0",
                           "evdoa", "evdob", "ehrpd", "lte"};

[Pref="dom.mobileconnection.enabled"]
interface MozMobileConnectionInfo
{
  /**
   * State of the connection.
   */
  readonly attribute MobileConnectionState? state;

  /**
   * Indicates whether the connection is ready.
   *
   * Note: The meaning of "connection ready" for data and voice are different.
   *       - Data: the "default" data connection is established or not.
   *       - Voice: voice is registered to network or not.
   */
  readonly attribute boolean connected;

  /**
   * Indicates whether only emergency calls are possible.
   *
   * This flag is only relevant to voice connections and when 'connected' is
   * false.
   */
  readonly attribute boolean emergencyCallsOnly;

  /**
   * Indicates whether the connection is going through a foreign operator
   * (roaming) or not.
   */
  readonly attribute boolean roaming;

  /**
   * Network operator information.
   */
  readonly attribute MozMobileNetworkInfo? network;

  /**
   * Type of connection.
   */
  readonly attribute MobileConnectionType? type;

  /**
   * Signal strength in dBm, or null if no service is available.
   */
  readonly attribute long? signalStrength;

  /**
   * Signal strength, represented linearly as a number between 0 (weakest
   * signal) and 100 (full signal).
   */
  readonly attribute unsigned short? relSignalStrength;

  /**
   * Cell location information.
   */
  readonly attribute MozMobileCellInfo? cell;
};
