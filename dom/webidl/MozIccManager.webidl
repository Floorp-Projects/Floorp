/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.icc.enabled"]
interface MozIccManager : EventTarget
{
  /**
   * STK menu presentation types.
   */
  const unsigned short STK_MENU_TYPE_NOT_SPECIFIED      = 0x00;
  const unsigned short STK_MENU_TYPE_DATA_VALUES        = 0x01;
  const unsigned short STK_MENU_TYPE_NAVIGATION_OPTIONS = 0x03;

  /**
   * Browser launch mode.
   */
  const unsigned short STK_BROWSER_MODE_LAUNCH_IF_NOT_ALREADY_LAUNCHED = 0x00;
  const unsigned short STK_BROWSER_MODE_USING_EXISTING_BROWSER         = 0x02;
  const unsigned short STK_BROWSER_MODE_USING_NEW_BROWSER              = 0x03;

  /**
   * STK proactive commands.
   *
   * @see TS 11.14, clause 13.4
   */
  const unsigned short STK_CMD_REFRESH               = 0x01;
  const unsigned short STK_CMD_POLL_INTERVAL         = 0x03;
  const unsigned short STK_CMD_POLL_OFF              = 0x04;
  const unsigned short STK_CMD_SET_UP_EVENT_LIST     = 0x05;
  const unsigned short STK_CMD_SET_UP_CALL           = 0x10;
  const unsigned short STK_CMD_SEND_SS               = 0x11;
  const unsigned short STK_CMD_SEND_USSD             = 0x12;
  const unsigned short STK_CMD_SEND_SMS              = 0x13;
  const unsigned short STK_CMD_SEND_DTMF             = 0x14;
  const unsigned short STK_CMD_LAUNCH_BROWSER        = 0x15;
  const unsigned short STK_CMD_PLAY_TONE             = 0x20;
  const unsigned short STK_CMD_DISPLAY_TEXT          = 0x21;
  const unsigned short STK_CMD_GET_INKEY             = 0x22;
  const unsigned short STK_CMD_GET_INPUT             = 0x23;
  const unsigned short STK_CMD_SELECT_ITEM           = 0x24;
  const unsigned short STK_CMD_SET_UP_MENU           = 0x25;
  const unsigned short STK_CMD_PROVIDE_LOCAL_INFO    = 0x26;
  const unsigned short STK_CMD_TIMER_MANAGEMENT      = 0x27;
  const unsigned short STK_CMD_SET_UP_IDLE_MODE_TEXT = 0x28;
  const unsigned short STK_CMD_OPEN_CHANNEL          = 0x40;
  const unsigned short STK_CMD_CLOSE_CHANNEL         = 0x41;
  const unsigned short STK_CMD_RECEIVE_DATA          = 0x42;
  const unsigned short STK_CMD_SEND_DATA             = 0x43;

  /**
   * STK result code.
   *
   * @see TS 11.14, clause 12.12
   *
   * Results '0X' and '1X' indicate that the command has been performed.
   */
  /** Command performed successfully */
  const unsigned short STK_RESULT_OK                                = 0x00;

  /** Command performed with partial comprehension */
  const unsigned short STK_RESULT_PRFRMD_WITH_PARTIAL_COMPREHENSION = 0x01;

  /** Command performed, with missing information */
  const unsigned short STK_RESULT_PRFRMD_WITH_MISSING_INFO          = 0x02;

  /** REFRESH performed with additional EFs read */
  const unsigned short STK_RESULT_PRFRMD_WITH_ADDITIONAL_EFS_READ   = 0x03;

  /** Command performed successfully, limited service */
  const unsigned short STK_RESULT_PRFRMD_LIMITED_SERVICE            = 0x06;

  /** Proactive UICC session terminated by the user */
  const unsigned short STK_RESULT_UICC_SESSION_TERM_BY_USER         = 0x10;

  /** Backward move in the proactive UICC session requested by the user */
  const unsigned short STK_RESULT_BACKWARD_MOVE_BY_USER             = 0x11;

  /** No response from user */
  const unsigned short STK_RESULT_NO_RESPONSE_FROM_USER             = 0x12;

  /** Help information required by the user */
  const unsigned short STK_RESULT_HELP_INFO_REQUIRED                = 0x13;

  /** USSD or SS transaction terminated by the user */
  const unsigned short STK_RESULT_USSD_SS_SESSION_TERM_BY_USER      = 0x14;

  /**
   * Results '2X' indicate to the UICC that it may be worth re-trying the
   * command at a later opportunity.
   */
  /** Terminal currently unable to process command */
  const unsigned short STK_RESULT_TERMINAL_CRNTLY_UNABLE_TO_PROCESS = 0x20;

  /** Network currently unable to process command */
  const unsigned short STK_RESULT_NETWORK_CRNTLY_UNABLE_TO_PROCESS  = 0x21;

  /** User did not accept the proactive command */
  const unsigned short STK_RESULT_USER_NOT_ACCEPT                   = 0x22;

  /** User cleared down call before connection or network release */
  const unsigned short STK_RESULT_USER_CLEAR_DOWN_CALL              = 0x23;

  /** Launch browser generic error code */
  const unsigned short STK_RESULT_LAUNCH_BROWSER_ERROR              = 0x26;

  /**
   * Results '3X' indicate that it is not worth the UICC re-trying with an
   * identical command, as it will only get the same response. However, the
   * decision to retry lies with the application.
   */
  /** Command beyond terminal's capabilities */
  const unsigned short STK_RESULT_BEYOND_TERMINAL_CAPABILITY        = 0x30;

  /** Command type not understood by terminal */
  const unsigned short STK_RESULT_CMD_TYPE_NOT_UNDERSTOOD           = 0x31;

  /** Command data not understood by terminal */
  const unsigned short STK_RESULT_CMD_DATA_NOT_UNDERSTOOD           = 0x32;

  /** Command number not known by terminal */
  const unsigned short STK_RESULT_CMD_NUM_NOT_KNOWN                 = 0x33;

  /** SS return error */
  const unsigned short STK_RESULT_SS_RETURN_ERROR                   = 0x34;

  /** SMS RP-ERROR */
  const unsigned short STK_RESULT_SMS_RP_ERROR                      = 0x35;

  /** Error, required values are missing */
  const unsigned short STK_RESULT_REQUIRED_VALUES_MISSING           = 0x36;

  /** USSD return error */
  const unsigned short STK_RESULT_USSD_RETURN_ERROR                 = 0x37;

  /** MultipleCard commands error */
  const unsigned short STK_RESULT_MULTI_CARDS_CMD_ERROR             = 0x38;

  /**
   * Interaction with call control by USIM or MO short message control by
   * USIM, permanent problem.
   */
  const unsigned short STK_RESULT_USIM_CALL_CONTROL_PERMANENT       = 0x39;

  /** Bearer independent protocol error */
  const unsigned short STK_RESULT_BIP_ERROR                         = 0x3a;

  /**
   * STK event list.
   */
  const unsigned short STK_EVENT_TYPE_MT_CALL                          = 0x00;
  const unsigned short STK_EVENT_TYPE_CALL_CONNECTED                   = 0x01;
  const unsigned short STK_EVENT_TYPE_CALL_DISCONNECTED                = 0x02;
  const unsigned short STK_EVENT_TYPE_LOCATION_STATUS                  = 0x03;
  const unsigned short STK_EVENT_TYPE_USER_ACTIVITY                    = 0x04;
  const unsigned short STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE            = 0x05;
  const unsigned short STK_EVENT_TYPE_CARD_READER_STATUS               = 0x06;
  const unsigned short STK_EVENT_TYPE_LANGUAGE_SELECTION               = 0x07;
  const unsigned short STK_EVENT_TYPE_BROWSER_TERMINATION              = 0x08;
  const unsigned short STK_EVENT_TYPE_DATA_AVAILABLE                   = 0x09;
  const unsigned short STK_EVENT_TYPE_CHANNEL_STATUS                   = 0x0a;
  const unsigned short STK_EVENT_TYPE_SINGLE_ACCESS_TECHNOLOGY_CHANGED = 0x0b;
  const unsigned short STK_EVENT_TYPE_DISPLAY_PARAMETER_CHANGED        = 0x0c;
  const unsigned short STK_EVENT_TYPE_LOCAL_CONNECTION                 = 0x0d;
  const unsigned short STK_EVENT_TYPE_NETWORK_SEARCH_MODE_CHANGED      = 0x0e;
  const unsigned short STK_EVENT_TYPE_BROWSING_STATUS                  = 0x0f;
  const unsigned short STK_EVENT_TYPE_FRAMES_INFORMATION_CHANGED       = 0x10;

  /**
   * The service state of STK location status.
   */
  const unsigned short STK_SERVICE_STATE_NORMAL      = 0x00;
  const unsigned short STK_SERVICE_STATE_LIMITED     = 0x01;
  const unsigned short STK_SERVICE_STATE_UNAVAILABLE = 0x02;

  /**
   * Tone type.
   */
  const unsigned short STK_TONE_TYPE_DIAL_TONE                = 0x01;
  const unsigned short STK_TONE_TYPE_CALLED_SUBSCRIBER_BUSY   = 0x02;
  const unsigned short STK_TONE_TYPE_CONGESTION               = 0x03;
  const unsigned short STK_TONE_TYPE_RADIO_PATH_ACK           = 0x04;
  const unsigned short STK_TONE_TYPE_RADIO_PATH_NOT_AVAILABLE = 0x05;
  const unsigned short STK_TONE_TYPE_ERROR                    = 0x06;
  const unsigned short STK_TONE_TYPE_CALL_WAITING_TONE        = 0x07;
  const unsigned short STK_TONE_TYPE_RINGING_TONE             = 0x08;
  const unsigned short STK_TONE_TYPE_GENERAL_BEEP             = 0x10;
  const unsigned short STK_TONE_TYPE_POSITIVE_ACK_TONE        = 0x11;
  const unsigned short STK_TONE_TYPE_NEGATIVE_ACK_TONE        = 0x12;

  /**
   * Time unit.
   */
  const unsigned short STK_TIME_UNIT_MINUTE       = 0x00;
  const unsigned short STK_TIME_UNIT_SECOND       = 0x01;
  const unsigned short STK_TIME_UNIT_TENTH_SECOND = 0x02;

  /**
   * Local Information list.
   *
   * @see TS 102.223, clause 8.6
   */
  const unsigned short STK_LOCAL_INFO_LOCATION_INFO  = 0x00;
  const unsigned short STK_LOCAL_INFO_IMEI           = 0x01;
  const unsigned short STK_LOCAL_INFO_DATE_TIME_ZONE = 0x03;
  const unsigned short STK_LOCAL_INFO_LANGUAGE       = 0x04;

  /**
   * Timer management.
   */
  const unsigned short STK_TIMER_START             = 0x00;
  const unsigned short STK_TIMER_DEACTIVATE        = 0x01;
  const unsigned short STK_TIMER_GET_CURRENT_VALUE = 0x02;

  /**
   * Browser termination cause.
   */
  const unsigned short STK_BROWSER_TERMINATION_CAUSE_USER  = 0x00;
  const unsigned short STK_BROWSER_TERMINATION_CAUSE_ERROR = 0x01;

  /**
   * Next Action Indicator.
   */
  const unsigned short STK_NEXT_ACTION_NULL                  = 0x00;
  const unsigned short STK_NEXT_ACTION_END_PROACTIVE_SESSION = 0x81;

  /**
   * Array of iccIds that are currently detected.
   */
  [Cached, Pure]
  readonly attribute sequence<DOMString> iccIds;

  /**
   * Get ICC object by iccId.
   *
   * @param iccId
   *        The identifier of the ICC.
   *
   * @return see MozIcc.webidl for the detail.
   */
  MozIcc? getIccById(DOMString iccId);

  /**
   * 'oniccdetected' event is notified whenever a new ICC is detected.
   */
  attribute EventHandler oniccdetected;

  /**
   * 'oniccundetected' event is notified whenever an ICC becomes undetected.
   */
  attribute EventHandler oniccundetected;
};
