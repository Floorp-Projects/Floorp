/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum IccType {"sim", "usim", "csim", "ruim"};

[Pref="dom.icc.enabled",
 ChromeOnly]
interface MozIccInfo {
  /**
   * Integrated Circuit Card Type.
   */
  readonly attribute IccType? iccType;

  /**
   * Integrated Circuit Card Identifier.
   */
  readonly attribute DOMString? iccid;

  /**
   * Mobile Country Code (MCC) of the subscriber's home network.
   */
  readonly attribute DOMString? mcc;

  /**
   * Mobile Network Code (MNC) of the subscriber's home network.
   */
  readonly attribute DOMString? mnc;

  /**
   * Service Provider Name (SPN) of the subscriber's home network.
   */
  readonly attribute DOMString? spn;

  /**
   * Network name must be a part of displayed carrier name.
   */
  readonly attribute boolean isDisplayNetworkNameRequired;

  /**
   * Service provider name must be a part of displayed carrier name.
   */
  readonly attribute boolean isDisplaySpnRequired;
};

[Pref="dom.icc.enabled",
 ChromeOnly]
interface MozGsmIccInfo : MozIccInfo {
  /**
   * Mobile Station ISDN Number (MSISDN) of the subscriber, aka
   * his phone number.
   */
  readonly attribute DOMString? msisdn;
};

[Pref="dom.icc.enabled",
 ChromeOnly]
interface MozCdmaIccInfo : MozIccInfo {
  /**
   * Mobile Directory Number (MDN) of the subscriber, aka his phone number.
   */
  readonly attribute DOMString? mdn;

  /**
   * Preferred Roaming List (PRL) version of the subscriber.
   */
  readonly attribute long prlVersion;
};
