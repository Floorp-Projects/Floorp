/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum IccImageCodingScheme { "basic", "color", "color-transparency" };

dictionary MozStkIcon
{
  /*
   * Width of the icon.
   */
  unsigned long width;

  /*
   * Height of the icon.
   */
  unsigned long height;

  /*
   * Image coding scheme of the icon.
   */
  IccImageCodingScheme codingScheme;

  /*
   * Array of pixels. Each pixel represents a color in the RGBA format made up
   * of four bytes, that is, the Red sample in the highest 8 bits, followed by
   * the Green sample, Blue sample and the Alpha sample in the lowest 8 bits.
   */
  sequence<unsigned long> pixels;
};

dictionary MozStkIconContainer
{
  /**
   * Indicates how the icon is to be used.
   *
   * @see TS 11.14, clause 12.31, Icon Identifier.
   *
   * true: icon replaces the text string.
   * false: icon shall be displayed together with the text string.
   */
  boolean iconSelfExplanatory;

  /**
   * Icon(s) that replaces or accompanies the text string.
   *
   * @see TS 11.14, clause 12.31, Icon Identifier.
   *
   * Array of icons, basically of a same image, that may differ in size,
   * resolution or coding scheme. The first icon should be the default one.
   */
  sequence<MozStkIcon> icons;
};

[Pref="dom.icc.enabled",
 CheckPermissions="mobileconnection",
 AvailableIn="CertifiedApps",
 Constructor(DOMString type, optional MozStkCommandEventInit eventInitDict)]
interface MozStkCommandEvent : Event
{
  readonly attribute any command;
};

dictionary MozStkCommandEventInit : EventInit
{
  any command = null;
};

dictionary MozStkTextMessage : MozStkIconContainer
{
  /**
   * Text String.
   *
   * @see TS 11.14, clause 12.15, Text String.
   */
  DOMString text;

  /**
   * The length of time for which the ME shall display the dialog.
   */
  MozStkDuration duration;

  /**
   * Indicate this text message is high priority or normal priority.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, Display Text, bit 1.
   *
   * High priority text shall be displayed on the screen immediately, except if
   * there is a conflict of priority level of alerting such as incoming calls
   * or a low battery warning. In that situation, the resolution is left to
   * the terminal. If the command is rejected in spite of the high priority,
   * the terminal shall inform the ICC with resultCode is
   * MozIccManager.STK_RESULT_TERMINAL_CRNTLY_UNABLE_TO_PROCESS in MozStkResponse.
   *
   * true: high priority
   * false: normal priority
   */
  boolean isHighPriority;

  /**
   * Need to wait for user to clear message or not.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, Display Text, bit 8.
   *
   * If this attribute is true, but user doesn't give any input within a period
   * of time(said 30 secs), the terminal shall inform the ICC with resultCode
   * is MozIccManager.STK_RESULT_NO_RESPONSE_FROM_USER in MozStkResponse.
   *
   * true: Wait for user to clear message.
   * false: clear message after a delay.
   */
  boolean userClear;

  /**
   * Need to response immediately or not.
   *
   * @see TS 11.14, clause 12.43, Immediate response.
   *
   * When this attribute is true, the terminal shall immediately send
   * MozStkResponse with resultCode is OK.
   *
   * true: The terminal shall send response immediately.
   * false: otherwise.
   */
  boolean responseNeeded;
};

dictionary MozStkItem : MozStkIconContainer
{
  /**
   * Identifier of item.
   *
   * The identifier is a single byte between '01' and 'FF'. Each item shall
   * have a unique identifier within an Item list.
   */
  unsigned short identifier;

  /**
   * Text string of item.
   */
  DOMString text;
};

dictionary MozStkMenu : MozStkIconContainer
{
  /**
   * Array of MozStkItem.
   *
   * @see TS 11.14, clause 12.9
   */
  sequence<MozStkItem> items;

  /**
   * Presentation type, one of MozIccManager.STK_MENU_TYPE_*.
   */
  unsigned short presentationType;

  /**
   * Title of the menu.
   */
  DOMString title;

  /**
   * Default item identifier of the menu.
   */
  unsigned short defaultItem;

  /**
   * Help information available or not.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, SET UP MENU, bit 8.
   *
   * true: help information available.
   * false: no help information available.
   */
  boolean isHelpAvailable;

  /**
   * List of Next Action Indicators.
   * Each element should be one of MozIccManager.STK_CMD_*
   *                            or MozIccManager.STK_NEXT_ACTION_*
   * If it's MozIccManager.STK_NEXT_ACTION_NULL, the terminal should ignore this
   * action in corresponding item.
   *
   * @see TS 11.14, clause 12.24, Items Next Action Indicator.
   */
  sequence<unsigned short> nextActionList;
};

dictionary MozStkInput : MozStkIconContainer
{
  /**
   * Text for the ME to display in conjunction with asking the user to respond.
   */
  DOMString text;

  /**
   * The length of time for which the ME shall display the dialog. This field
   * is used only for GET INKEY.
   *
   * @see TS 11.14, clause 11.8, duration, GET INKEY T.C 27.22.4.2.8.4.2
   */
  MozStkDuration duration;

  /**
   * Minimum length of response.
   */
  unsigned short minLength;

  /**
   * Maximum length of response.
   */
  unsigned short maxLength;

  /**
   * Text for the ME to display, corresponds to a default text string offered
   * by the ICC.
   */
  DOMString defaultText;

  /**
   * Input format.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, GET INPUT, bit 1.
   *
   * true: Alphabet set.
   * false: Digits only.
   */
  boolean isAlphabet;

  /**
   * Alphabet encoding.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, GET INPUT, bit 2.
   *
   * true: UCS2 alphabet.
   * false: default SMS alphabet.
   */
  boolean isUCS2;

  /**
   * Visibility of input.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, GET INPUT, bit 3.
   *
   * true: User input shall not be revealed in any way.
   * false: ME may echo user input on the display.
   */
  boolean hideInput;

  /**
   * Yes/No response is requested.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, GET INKEY, bit 3.
   *
   * true: Yes/No response is requested, and character sets
   *       (Alphabet set and UCS2) are disabled.
   * false: Character sets (Alphabet set and UCS2) are enabled.
   */
  boolean isYesNoRequested;

  /**
   * User input in packed or unpacked format.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, GET INPUT, bit 4.
   *
   * true: User input to be in SMS packed format.
   * false: User input to be in unpacked format.
   */
  boolean isPacked;

  /**
   * Help information available or not.
   *
   * @see TS 11.14, clause 12.6, Command Qualifier, GET INPUT/GET INKEY, bit 8.
   *
   * true: help information available.
   * false: no help information available.
   */
  boolean isHelpAvailable;
};

dictionary MozStkBrowserSetting
{
  /**
   * Confirm message to launch browser.
   */
  MozStkTextMessage confirmMessage;

  /**
   * The URL to be opened by browser.
   */
  DOMString url;

  /**
   * One of MozIccManager.STK_BROWSER_MODE_*.
   */
  unsigned short mode;
};

dictionary MozStkSetUpCall
{
  /**
   * The Dialling number.
   */
  DOMString address;

  /**
   * The text message used in user confirmation phase.
   */
  MozStkTextMessage confirmMessage;

  /**
   * The text message used in call set up phase.
   */
  MozStkTextMessage callMessage;

  /**
   * The Optional maximum duration for the redial mechanism.
   * The time elapsed since the first call set-up attempt has exceeded the duration
   * requested by the UICC, the redial mechanism is terminated.
   */
  MozStkDuration duration;
};

dictionary MozStkSetUpEventList
{
  /**
   * The list of events that needs to provide details to ICC when they happen.
   * When this valus is null, means an indication to remove the existing list
   * of events in ME.
   *
   * @see MozIccManager.STK_EVENT_TYPE_*
   */
   sequence<unsigned short> eventList;
};

dictionary MozStkLocationInfo
{
  /**
   * Mobile Country Code (MCC) of the current serving operator.
   */
  unsigned short mcc;

  /**
   * Mobile Network Code (MNC) of the current serving operator.
   */
  unsigned short mnc;

  /**
   * Mobile Location Area Code (LAC) for the current serving operator.
   */
  unsigned short gsmLocationAreaCode;

  /**
   * Mobile Cell ID for the current serving operator.
   */
  unsigned long gsmCellId;
};

dictionary MozStkDuration
{
  /**
   * Time unit used, should be one of MozIccManager.STK_TIME_UNIT_*.
   */
  unsigned short timeUnit;

  /**
   * The length of time required, expressed in timeUnit.
   */
  octet timeInterval;
};

dictionary MozStkPlayTone : MozStkIconContainer
{
  /**
   * Text String.
   */
  DOMString text;

  /**
   * One of MozIccManager.STK_TONE_TYPE_*.
   */
  unsigned short tone;

  /**
   * The length of time for which the ME shall generate the tone.
   */
  MozStkDuration duration;

  /**
   * Need to vibrate or not.
   * true: vibrate alert, if available, with the tone.
   * false: use of vibrate alert is up to the ME.
   */
  boolean isVibrate;
};

dictionary MozStkProvideLocalInfo
{
  /**
   * Indicate which local information is required.
   * It shall be one of following:
   *  - MozIccManager.STK_LOCAL_INFO_LOCATION_INFO
   *  - MozIccManager.STK_LOCAL_INFO_IMEI
   *  - MozIccManager.STK_LOCAL_INFO_DATE_TIME_ZONE
   *  - MozIccManager.STK_LOCAL_INFO_LANGUAGE
   */
  unsigned short localInfoType;
};

dictionary MozStkLocationEvent
{
  /**
   * The type of this event.
   * It shall be MozIccManager.STK_EVENT_TYPE_LOCATION_STATUS;
   */
  unsigned short eventType;

  /**
   * Indicate current service state of the MS with one of the values listed
   * below:
   *  - MozIccManager.STK_SERVICE_STATE_NORMAL
   *  - MozIccManager.STK_SERVICE_STATE_LIMITED
   *  - MozIccManager.STK_SERVICE_STATE_UNAVAILABLE
   */
  unsigned short locationStatus;

  /**
   * See MozStkLocationInfo.
   * This value shall only be provided if the locationStatus indicates
   * MozIccManager.STK_SERVICE_STATE_NORMAL.
   */
  MozStkLocationInfo locationInfo;
};

dictionary MozStkTimer
{
  /**
   * Identifier of a timer.
   */
  octet timerId;

  /**
   * Length of time during which the timer has to run.
   * The resolution of a timer is 1 second.
   */
  unsigned long timerValue;

  /**
   * The action requested from UICC.
   * It shall be one of below:
   * - MozIccManager.STK_TIMER_START
   * - MozIccManager.STK_TIMER_DEACTIVATE
   * - MozIccManager.STK_TIMER_GET_CURRENT_VALUE
   */
  unsigned short timerAction;
};

dictionary MozStkBipMessage : MozStkIconContainer
{
  /**
   * Text String
   */
  DOMString text;
};

dictionary MozStkCommand
{
  /**
   * The number of command issued by ICC. And it is assigned
   * by ICC may take any hexadecimal value betweean '01' and 'FE'.
   *
   * @see TS 11.14, clause 6.5.1
   */
  unsigned short commandNumber;

  /**
   * One of MozIccManager.STK_CMD_*
   */
  unsigned short typeOfCommand;

  /**
   * Qualifiers specific to the command.
   */
  unsigned short commandQualifier;

  /**
   * options varies accrording to the typeOfCommand in MozStkCommand.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_DISPLAY_TEXT
   * - MozIccManager.STK_CMD_SET_UP_IDLE_MODE_TEXT
   * - MozIccManager.STK_CMD_SEND_{SS|USSD|SMS|DTMF},
   * options is MozStkTextMessage.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_SELECT_ITEM
   * - MozIccManager.STK_CMD_SET_UP_MENU
   * options is MozStkMenu.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_GET_INKEY
   * - MozIccManager.STK_CMD_GET_INPUT,
   * options is MozStkInput.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_LAUNCH_BROWSER
   * options is MozStkBrowserSetting.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_SET_UP_CALL
   * options is MozStkSetUpCall.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_SET_UP_EVENT_LIST
   * options is MozStkSetUpEventList.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_PLAY_TONE
   * options is MozStkPlayTone.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_POLL_INTERVAL
   * options is MozStkDuration.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_PROVIDE_LOCAL_INFO
   * options is MozStkProvideLocalInfo.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_TIMER_MANAGEMENT
   * option is MozStkTimer
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_OPEN_CHANNEL
   * - MozIccManager.STK_CMD_CLOSE_CHANNEL
   * - MozIccManager.STK_CMD_SEND_DATA
   * - MozIccManager.STK_CMD_RECEIVE_DATA
   * options is MozStkBipMessage
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_POLL_OFF
   * options is null.
   *
   * When typeOfCommand is
   * - MozIccManager.STK_CMD_REFRESH
   * options is null.
   */
  any options;
};

dictionary MozStkResponse
{
  /**
   * One of MozIccManager.STK_RESULT_*
   */
  unsigned short resultCode;

  /**
   * One of MozIccManager.STK_ADDITIONAL_INFO_*
   */
  unsigned short additionalInformation;

  /**
   * The identifier of the item selected by user.
   *
   * @see MozStkItem.identifier
   */
  unsigned short itemIdentifier;

  /**
   * User input.
   */
  DOMString input;

  /**
   * YES/NO response.
   *
   * @see MozStkInput.isYesNoRequested
   */
  boolean isYesNo;

  /**
   * User has confirmed or rejected the call during
   * MozIccManager.STK_CMD_CALL_SET_UP.
   *
   * @see RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM
   *
   * true: Confirmed by User.
   * false: Rejected by User.
   */
  boolean hasConfirmed;

  /**
   * The response for MozIccManager.STK_CMD_PROVIDE_LOCAL_INFO
   */
  MozStkLocalInfo localInfo;

  /**
   * The response for MozIccManager.STK_CMD_TIMER_MANAGEMENT.
   * The 'timerValue' is needed if the action of
   * MozIccManager.STK_CMD_TIMER_MANAGEMENT is MozIccManager.STK_TIMER_DEACTIVATE
   * or MozIccManager.STK_TIMER_GET_CURRENT_VALUE.
   * It shall state the current value of a timer. And the resolution is 1 second.
   */
  MozStkTimer timer;
};

dictionary MozStkCallEvent
{
  /**
   * The type of this event.
   * It shall be one of following:
   *     - MozIccManager.STK_EVENT_TYPE_MT_CALL,
   *     - MozIccManager.STK_EVENT_TYPE_CALL_CONNECTED,
   *     - MozIccManager.STK_EVENT_TYPE_CALL_DISCONNECTED.
   */
  unsigned short eventType;

  /**
   * Remote party number.
   */
  DOMString number;

  /**
   * This field is available in MozIccManager.STK_EVENT_TYPE_CALL_CONNECTED and
   * MozIccManager.STK_EVENT_TYPE_CALL_DISCONNECTED events.
   * For the MozIccManager.STK_EVENT_TYPE_CALL_CONNECTED event, setting this to
   * true means the connection is answered by remote end, that is, this is an
   * outgoing call.
   * For the MozIccManager.STK_EVENT_TYPE_CALL_DISCONNECTED event, setting this
   * to true indicates the connection is hung up by remote.
   */
  boolean isIssuedByRemote;

  /**
   * This field is available in Call Disconnected event to indicate the cause
   * of disconnection. The cause string is passed to gaia through the error
   * listener of CallEvent. Null if there's no error.
   */
  DOMString error;
};

dictionary MozStkLocalInfo
{
  /**
   * IMEI information
   */
  DOMString imei;

  /**
   * Location Information
   */
  MozStkLocationInfo locationInfo;

  /**
   * Date information
   */
  Date date;

  /**
   * Language Information
   *
   * @see ISO 639-1, Alpha-2 code
   */
  DOMString language;
};

dictionary MozStkLanguageSelectionEvent
{
  /**
   * The type of this event.
   * It shall be MozIccManager.STK_EVENT_TYPE_LANGUAGE_SELECTION.
   */
  unsigned short eventType;

  /**
   * Language Information
   *
   * @see ISO 639-1, Alpha-2 code
   *      "de" for German, "en" for English, "zh" for Chinese, etc.
   */
  DOMString language;
};

dictionary MozStkBrowserTerminationEvent
{
  /**
   * The type of this event.
   * It shall be MozIccManager.STK_EVENT_TYPE_BROWSER_TERMINATION
   */
  unsigned short eventType;

  /**
   * This object shall contain the browser termination cause.
   * See TZ 102 223 8.51. It shall be one of following:
   * - MozIccManager.STK_BROWSER_TERMINATION_CAUSE_USER
   * - MozIccManager.STK_BROWSER_TERMINATION_CAUSE_ERROR
   */
  unsigned short terminationCause;
};

dictionary MozStkGeneralEvent
{
  /**
   * The type of this event, MozStkGeneralEvent can be used for all Stk Event
   * requires no more parameter than event type, including
   * MozIccManager.STK_EVENT_TYPE_USER_ACTIVITY.
   * MozIccManager.STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE.
   * HCI Connectivity Event(Not defined in interface yet).
   */
  unsigned short eventType;
};
