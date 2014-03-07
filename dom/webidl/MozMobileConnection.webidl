/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


dictionary MozCallBarringOption
{
  /**
   * Indicates the program the call is being barred.
   *
   * It shall be one of the nsIDOMMozMobileConnection.CALL_BARRING_PROGRAM_*
   * values.
   */
   unsigned short program;

  /**
   * Enable or disable the call barring program.
   */
  boolean enabled;

  /**
   * Barring password. Use "" if no password specified.
   */
  DOMString password;

  /**
   * Service for which the call barring is set up.
   *
   * It shall be one of the nsIDOMMozMobileConnection.ICC_SERVICE_CLASS_*
   * values.
   */
  unsigned short serviceClass;
};

dictionary DOMMMIResult
{
  /**
   * String key that identifies the service associated with the MMI code
   * request. The UI is supposed to handle the localization of the strings
   * associated with this string key.
   */
  DOMString serviceCode;

  /**
   * String key containing the status message of the associated MMI request.
   * The UI is supposed to handle the localization of the strings associated
   * with this string key.
   */
  DOMString statusMessage;

  /**
   * Some MMI requests like call forwarding or PIN/PIN2/PUK/PUK2 related
   * requests provide extra information along with the status message, this
   * information can be a number, a string key or an array of string keys.
   */
  any additionalInformation;
};

dictionary DOMCLIRStatus
{
  /**
   * CLIR parameter 'n': parameter sets the adjustment for outgoing calls.
   *
   * 0 Presentation indicator is used according to the subscription of the
   *   CLIR service (uses subscription default value).
   * 1 CLIR invocation (restricts CLI presentation).
   * 2 CLIR suppression (allows CLI presentation).
   */
  unsigned short n;

  /**
   * CLIR parameter 'm': parameter shows the subscriber CLIR service status in
   *                     the network.
   * 0 CLIR not provisioned.
   * 1 CLIR provisioned in permanent mode.
   * 2 unknown (e.g. no network, etc.).
   * 3 CLIR temporary mode presentation restricted.
   *
   * @see 3GPP TS 27.007 7.7 Defined values
   */
  unsigned short m;
};
