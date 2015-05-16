/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum MobileNetworkSelectionMode {"automatic", "manual"};
enum MobileRadioState {"enabling", "enabled", "disabling", "disabled"};
enum MobileNetworkType {"gsm", "wcdma", "cdma", "evdo", "lte"};
enum MobilePreferredNetworkType {"wcdma/gsm", "gsm", "wcdma", "wcdma/gsm-auto",
                                 "cdma/evdo", "cdma", "evdo",
                                 "wcdma/gsm/cdma/evdo", "lte/cdma/evdo",
                                 "lte/wcdma/gsm", "lte/wcdma/gsm/cdma/evdo",
                                 "lte", "lte/wcdma"};
enum MobileRoamingMode {"home", "affiliated", "any"};

[Pref="dom.mobileconnection.enabled"]
interface MozMobileConnection : EventTarget
{
  const long ICC_SERVICE_CLASS_VOICE      = 0x01; // (1 << 0)
  const long ICC_SERVICE_CLASS_DATA       = 0x02; // (1 << 1)
  const long ICC_SERVICE_CLASS_FAX        = 0x04; // (1 << 2)
  const long ICC_SERVICE_CLASS_SMS        = 0x08; // (1 << 3)
  const long ICC_SERVICE_CLASS_DATA_SYNC  = 0x10; // (1 << 4)
  const long ICC_SERVICE_CLASS_DATA_ASYNC = 0x20; // (1 << 5)
  const long ICC_SERVICE_CLASS_PACKET     = 0x40; // (1 << 6)
  const long ICC_SERVICE_CLASS_PAD        = 0x80; // (1 << 7)
  const long ICC_SERVICE_CLASS_MAX        = 0x80; // (1 << 7)

  /**
   * Call forwarding action.
   *
   * @see 3GPP TS 27.007 7.11 "mode".
   */
  const long CALL_FORWARD_ACTION_DISABLE      = 0;
  const long CALL_FORWARD_ACTION_ENABLE       = 1;
  const long CALL_FORWARD_ACTION_QUERY_STATUS = 2;
  const long CALL_FORWARD_ACTION_REGISTRATION = 3;
  const long CALL_FORWARD_ACTION_ERASURE      = 4;

  /**
   * Call forwarding reason.
   *
   * @see 3GPP TS 27.007 7.11 "reason".
   */
  const long CALL_FORWARD_REASON_UNCONDITIONAL                   = 0;
  const long CALL_FORWARD_REASON_MOBILE_BUSY                     = 1;
  const long CALL_FORWARD_REASON_NO_REPLY                        = 2;
  const long CALL_FORWARD_REASON_NOT_REACHABLE                   = 3;
  const long CALL_FORWARD_REASON_ALL_CALL_FORWARDING             = 4;
  const long CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING = 5;

  /**
   * Call barring program.
   */
  const long CALL_BARRING_PROGRAM_ALL_OUTGOING                       = 0;
  const long CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL             = 1;
  const long CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME = 2;
  const long CALL_BARRING_PROGRAM_ALL_INCOMING                       = 3;
  const long CALL_BARRING_PROGRAM_INCOMING_ROAMING                   = 4;

  /**
   * Calling line identification restriction constants.
   *
   * @see 3GPP TS 27.007 7.7 Defined values.
   */
  const long CLIR_DEFAULT     = 0;
  const long CLIR_INVOCATION  = 1;
  const long CLIR_SUPPRESSION = 2;

  /**
   * These two fields can be accessed by privileged applications with the
   * 'mobilenetwork' permission.
   */
  [CheckPermissions="mobilenetwork"]
  readonly attribute DOMString lastKnownNetwork;
  [CheckPermissions="mobilenetwork"]
  readonly attribute DOMString lastKnownHomeNetwork;

  /**
   * Information about the voice connection.
   */
  [CheckPermissions="mobileconnection"]
  readonly attribute MozMobileConnectionInfo voice;

  /**
   * Information about the data connection.
   */
  [CheckPermissions="mobileconnection"]
  readonly attribute MozMobileConnectionInfo data;

  /**
   * Integrated Circuit Card Identifier of the SIM this mobile connection
   * corresponds to.
   */
  [CheckPermissions="mobileconnection"]
  readonly attribute DOMString? iccId;

  /**
   * The selection mode of the voice and data networks.
   */
  [CheckPermissions="mobileconnection"]
  readonly attribute MobileNetworkSelectionMode? networkSelectionMode;

  /**
   * The current radio state.
   */
  [CheckPermissions="mobileconnection"]
  readonly attribute MobileRadioState? radioState;

  /**
   * Array of network types that are supported by this radio.
   */
  [Cached, Pure, CheckPermissions="mobileconnection"]
  readonly attribute sequence<MobileNetworkType> supportedNetworkTypes;

  /**
   * Search for available networks.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an array of MozMobileNetworkInfo.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported', or
   * 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getNetworks();

  /**
   * Manually selects the passed in network, overriding the radio's current
   * selection.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   * Note: If the network was actually changed by this request,
   * the 'voicechange' and 'datachange' events will also be fired.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest selectNetwork(MozMobileNetworkInfo network);

  /**
   * Tell the radio to automatically select a network.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   * Note: If the network was actually changed by this request, the
   * 'voicechange' and 'datachange' events will also be fired.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest selectNetworkAutomatically();

  /**
   * Set preferred network type.
   *
   * @param type
   *        PreferredNetworkType indicates the desired preferred network type.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'ModeNotSupported', 'IllegalSIMorME', or
   * 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setPreferredNetworkType(MobilePreferredNetworkType type);

  /**
   * Query current preferred network type.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a string indicating the current preferred network type.
   * The value will be either 'wcdma/gsm', 'gsm', 'wcdma', 'wcdma/gsm-auto',
   * 'cdma/evdo', 'cdma', 'evdo', 'wcdma/gsm/cdma/evdo', 'lte/cdma/evdo',
   * 'lte/wcdma/gsm', 'lte/wcdma/gsm/cdma/evdo', 'lte' or 'lte/wcdma'.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getPreferredNetworkType();

  /**
   * Set roaming preference.
   *
   * @param mode
   *        RoamingPreferenceMode indicates the desired roaming preference.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setRoamingPreference(MobileRoamingMode mode);

  /**
   * Query current roaming preference.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a string indicating the current roaming preference.
   * The value will be either 'home', 'affiliated', or 'any'.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getRoamingPreference();

  /**
   * Set voice privacy preference.
   *
   * @param enabled
   *        Boolean indicates the preferred voice privacy mode used in voice
   *        scrambling in CDMA networks. 'True' means the enhanced voice security
   *        is required.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setVoicePrivacyMode(boolean enabled);

  /**
   * Query current voice privacy mode.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a boolean indicating the current voice privacy mode.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getVoicePrivacyMode();

  /**
   * Configures call forward options.
   *
   * @param options
   *        An object containing the call forward rule to set.
   * @see MozCallForwardingOptions for the detail of options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setCallForwardingOption(optional MozCallForwardingOptions options);

  /**
   * Queries current call forward options.
   *
   * @param reason
   *        Indicates the reason the call is being forwarded. It shall be one of
   *        the MozMobileConnection.CALL_FORWARD_REASON_* values.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an array of MozCallForwardingOptions.
   * @see MozCallForwardingOptions for the detail of result.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getCallForwardingOption(unsigned short reason);

  /**
   * Configures call barring options.
   *
   * @param options
   *        An object containing the call barring rule to set.
   * @see MozCallBarringOptions for the detail of options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setCallBarringOption(optional MozCallBarringOptions options);

  /**
   * Queries current call barring status.
   *
   * @param options
   *        An object containing the call barring rule to query. No need to
   *        specify 'enabled' property.
   * @see MozCallBarringOptions for the detail of options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an object of MozCallBarringOptions with correct 'enabled'
   * property indicating the status of this rule.
   * @see MozCallBarringOptions for the detail of result.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getCallBarringOption(optional MozCallBarringOptions options);

  /**
   * Change call barring facility password.
   *
   * @param options
   *        An object containing information about pin and newPin, and,
   *        this object must have both "pin" and "newPin" attributes
   *        to change the call barring facility password.
   * @see MozCallBarringOptions for the detail of options.
   *
   * Example:
   *
   *   changeCallBarringPassword({pin: "...",
   *                              newPin: "..."});
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest changeCallBarringPassword(optional MozCallBarringOptions options);

  /**
   * Configures call waiting options.
   *
   * @param enabled
   *        Boolean indicates the desired call waiting status.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setCallWaitingOption(boolean enabled);

  /**
   * Queries current call waiting options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a boolean indicating the call waiting status.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getCallWaitingOption();

  /**
   * Enables or disables the presentation of the calling line identity (CLI) to
   * the called party when originating a call.
   *
   * @param mode
   *        It shall be one of the MozMobileConnection.CLIR_* values.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setCallingLineIdRestriction(unsigned short mode);

  /**
   * Queries current CLIR status.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an object containing containing CLIR 'n' and 'm' parameter.
   * @see MozClirStatus for the detail of result.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest getCallingLineIdRestriction();

  /**
   * Exit emergency callback mode.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest exitEmergencyCbMode();

  /**
   * Set radio enabled/disabled.
   *
   * @param enabled
   *        True to enable the radio.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'InvalidStateError', 'RadioNotAvailable',
   * 'IllegalSIMorME', or 'GenericFailure'.
   *
   * Note: Request is not available when radioState is null, 'enabling', or
   * 'disabling'. Calling the function in above conditions will receive
   * 'InvalidStateError' error.
   */
  [Throws, CheckPermissions="mobileconnection"]
  DOMRequest setRadioEnabled(boolean enabled);

  /**
   * The 'voicechange' event is notified whenever the voice connection object
   * changes.
   */
  attribute EventHandler onvoicechange;

  /**
   * The 'datachange' event is notified whenever the data connection object
   * changes values.
   */
  attribute EventHandler ondatachange;

  /**
   * The 'dataerror' event is notified whenever the data connection object
   * receives an error from the RIL.
   */
  attribute EventHandler ondataerror;

  /**
   * The 'oncfstatechange' event is notified whenever the call forwarding
   * state changes.
   */
  attribute EventHandler oncfstatechange;

  /**
   * The 'emergencycbmodechange' event is notified whenever the emergency
   * callback mode changes.
   */
  attribute EventHandler onemergencycbmodechange;

  /**
   * The 'onotastatuschange' event is notified whenever the ota provision status
   * changes.
   */
  attribute EventHandler onotastatuschange;

  /**
   * The 'oniccchange' event is notified whenever the iccid value
   * changes.
   */
  attribute EventHandler oniccchange;

  /**
   * The 'onradiostatechange' event is notified whenever the radio state
   * changes.
   */
  attribute EventHandler onradiostatechange;

  /**
   * The 'onclirmodechange' event is notified whenever the mode of the calling
   * line id restriction (CLIR) changes.
   */
  attribute EventHandler onclirmodechange;
};

dictionary MozCallForwardingOptions
{
  /**
   * Call forwarding rule status.
   *
   * It will be either not active (false), or active (true).
   *
   * Note: Unused for setting call forwarding options. It reports
   *       the status of the rule when getting how the rule is
   *       configured.
   *
   * @see 3GPP TS 27.007 7.11 "status".
   */
  boolean? active;

  /**
   * Indicates what to do with the rule. It shall be one of the
   * MozMobileConnection.CALL_FORWARD_ACTION_* values.
   */
  unsigned short? action;

  /**
   * Indicates the reason the call is being forwarded. It shall be one of the
   * MozMobileConnection.CALL_FORWARD_REASON_* values.
   */
  unsigned short? reason;

  /**
   * Phone number of forwarding address.
   */
  DOMString? number;

  /**
   * When "no reply" is enabled or queried, this gives the time in
   * seconds to wait before call is forwarded.
   */
  unsigned short? timeSeconds;

  /**
   * Service for which the call forward is set up. It should be one of the
   * MozMobileConnection.ICC_SERVICE_CLASS_* values.
   */
  unsigned short? serviceClass;
};

dictionary MozCallBarringOptions
{
  /**
   * Indicates the program the call is being barred. It shall be one of the
   * MozMobileConnection.CALL_BARRING_PROGRAM_* values.
   */
  unsigned short? program;

  /**
   * Enable or disable the call barring program.
   */
  boolean? enabled;

  /**
   * Barring password. Use "" if no password specified.
   */
  DOMString? password;

  /**
   * Service for which the call barring is set up. It shall be one of the
   * MozMobileConnection.ICC_SERVICE_CLASS_* values.
   */
  unsigned short? serviceClass;

  /**
   * Old call barring password.
   *
   * Note: Only used for changeCallBarringPassword().
   */
  // TODO: Combine this with |password| and rename |newPin| to |newPassword|.
  //       But it needs to modify the gaia side as well, so we could consider
  //       doing this in bug 987541.
  DOMString? pin;

  /**
   * New call barring password.
   *
   * Note: Only used for changeCallBarringPassword().
   */
  DOMString? newPin;
};

dictionary MozMMIResult
{
  /**
   * Indicate whether the result is successful or not.
   */
  boolean success = true;

  /**
   * String key that identifies the service associated with the MMI code
   * request. The UI is supposed to handle the localization of the strings
   * associated with this string key.
   */
  DOMString serviceCode = "";

  /**
   * String key containing the status message of the associated MMI request or
   * the error message when the request fails.

   * The UI is supposed to handle the localization of the strings associated
   * with this string key.
   */
  DOMString statusMessage = "";

  /**
   * Some MMI requests like call forwarding or PIN/PIN2/PUK/PUK2 related
   * requests provide extra information along with the status message, this
   * information can be a number, an array of string keys or an array of
   * MozCallForwardingOptions.
   *
   * And it should be
   * (unsigned short or sequence<DOMString> or sequence<MozCallForwardingOptions>)
   * But we cannot yet use sequences as union member types (please see bug 767924)
   * ,so we use object here.
   */
  (unsigned short or object) additionalInformation;
};

dictionary MozClirStatus
{
  /**
   * CLIR parameter 'n': parameter sets the adjustment for outgoing calls.
   *
   * It shall be one of the MozMobileConnection.CLIR_* values.
   */
  unsigned short n;

  /**
   * CLIR parameter 'm': parameter shows the subscriber CLIR service status in
   *                     the network.
   * (0) CLIR not provisioned.
   * (1) CLIR provisioned in permanent mode.
   * (2) unknown (e.g. no network, etc.).
   * (3) CLIR temporary mode presentation restricted.
   *
   * @see 3GPP TS 27.007 7.7 defined values.
   */
  unsigned short m;
};
