/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum MobileNetworkState {"available", "connected", "forbidden"};

[Pref="dom.mobileconnection.enabled",
 ChromeConstructor(DOMString? shortName, DOMString? longName, DOMString? mcc,
                   DOMString? mnc, DOMString? state)]
interface MozMobileNetworkInfo
{
  /**
   * Short name of the network operator.
   */
  readonly attribute DOMString? shortName;

  /**
   * Long name of the network operator.
   */
  readonly attribute DOMString? longName;

  /**
   * Mobile Country Code (MCC) of the network operator.
   */
  readonly attribute DOMString? mcc;

  /**
   * Mobile Network Code (MNC) of the network operator.
   */
  readonly attribute DOMString? mnc;

  /**
   * State of this network operator.
   */
  readonly attribute MobileNetworkState? state;
};
