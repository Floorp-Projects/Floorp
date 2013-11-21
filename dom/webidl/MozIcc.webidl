/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface MozIccInfo;

[Pref="dom.icc.enabled"]
interface MozIcc : EventTarget
{
  // Integrated Circuit Card Information.

  /**
   * Information stored in the device's ICC.
   *
   * Once the ICC becomes undetectable, iccinfochange event will be notified.
   * Also, the attribute is set to null and this MozIcc object becomes invalid.
   * Calling asynchronous functions raises exception then.
   */
  readonly attribute MozIccInfo? iccInfo;

  /**
   * The 'iccinfochange' event is notified whenever the icc info object
   * changes.
   */
  attribute EventHandler oniccinfochange;

  // Integrated Circuit Card State.

  /**
   * Indicates the state of the device's ICC.
   *
   * Possible values: 'illegal', 'unknown', 'pinRequired',
   * 'pukRequired', 'personalizationInProgress', 'networkLocked',
   * 'corporateLocked', 'serviceProviderLocked', 'networkPukRequired',
   * 'corporatePukRequired', 'serviceProviderPukRequired',
   * 'personalizationReady', 'ready', 'permanentBlocked'.
   *
   * Once the ICC becomes undetectable, cardstatechange event will be notified.
   * Also, the attribute is set to null and this MozIcc object becomes invalid.
   * Calling asynchronous functions raises exception then.
   */
  readonly attribute DOMString? cardState;

  /**
   * The 'cardstatechange' event is notified when the 'cardState' attribute
   * changes value.
   */
  attribute EventHandler oncardstatechange;

  // Integrated Circuit Card STK.

  /**
   * Send the response back to ICC after an attempt to execute STK proactive
   * Command.
   *
   * @param command
   *        Command received from ICC. See MozStkCommand.
   * @param response
   *        The response that will be sent to ICC.
   *        @see MozStkResponse for the detail of response.
   */
  [Throws]
  void sendStkResponse(any command, any response);

  /**
   * Send the "Menu Selection" envelope command to ICC for menu selection.
   *
   * @param itemIdentifier
   *        The identifier of the item selected by user.
   * @param helpRequested
   *        true if user requests to provide help information, false otherwise.
   */
  [Throws]
  void sendStkMenuSelection(unsigned short itemIdentifier,
                            boolean helpRequested);

  /**
   * Send the "Timer Expiration" envelope command to ICC for TIMER MANAGEMENT.
   *
   * @param timer
   *        The identifier and value for a timer.
   *        timerId: Identifier of the timer that has expired.
   *        timerValue: Different between the time when this command is issued
   *                    and when the timer was initially started.
   *        @see MozStkTimer
   */
  [Throws]
  void sendStkTimerExpiration(any timer);

  /**
   * Send "Event Download" envelope command to ICC.
   * ICC will not respond with any data for this command.
   *
   * @param event
   *        one of events below:
   *        - MozStkLocationEvent
   *        - MozStkCallEvent
   *        - MozStkLanguageSelectionEvent
   *        - MozStkGeneralEvent
   *        - MozStkBrowserTerminationEvent
   */
  [Throws]
  void sendStkEventDownload(any event);

  /**
   * The 'stkcommand' event is notified whenever STK proactive command is
   * issued from ICC.
   */
  attribute EventHandler onstkcommand;

  /**
   * 'stksessionend' event is notified whenever STK session is terminated by
   * ICC.
   */
  attribute EventHandler onstksessionend;

  // Integrated Circuit Card Lock interfaces.

  /**
   * Find out about the status of an ICC lock (e.g. the PIN lock).
   *
   * @param lockType
   *        Identifies the lock type, e.g. "pin" for the PIN lock, "fdn" for
   *        the FDN lock.
   *
   * @return a DOMRequest.
   *         The request's result will be an object containing
   *         information about the specified lock's status,
   *         e.g. {lockType: "pin", enabled: true}.
   */
  [Throws]
  nsISupports getCardLock(DOMString lockType);

  /**
   * Unlock a card lock.
   *
   * @param info
   *        An object containing the information necessary to unlock
   *        the given lock. At a minimum, this object must have a
   *        "lockType" attribute which specifies the type of lock, e.g.
   *        "pin" for the PIN lock. Other attributes are dependent on
   *        the lock type.
   *
   * Examples:
   *
   * (1) Unlocking the PIN:
   *
   *   unlockCardLock({lockType: "pin",
   *                   pin: "..."});
   *
   * (2) Unlocking the PUK and supplying a new PIN:
   *
   *   unlockCardLock({lockType: "puk",
   *                   puk: "...",
   *                   newPin: "..."});
   *
   * (3) Network depersonalization. Unlocking the network control key (NCK).
   *
   *   unlockCardLock({lockType: "nck",
   *                   pin: "..."});
   *
   * (4) Corporate depersonalization. Unlocking the corporate control key (CCK).
   *
   *   unlockCardLock({lockType: "cck",
   *                   pin: "..."});
   *
   * (5) Service Provider depersonalization. Unlocking the service provider
   *     control key (SPCK).
   *
   *   unlockCardLock({lockType: "spck",
   *                   pin: "..."});
   *
   * (6) Network PUK depersonalization. Unlocking the network control key (NCK).
   *
   *   unlockCardLock({lockType: "nckPuk",
   *                   puk: "..."});
   *
   * (7) Corporate PUK depersonalization. Unlocking the corporate control key
   *     (CCK).
   *
   *   unlockCardLock({lockType: "cckPuk",
   *                   puk: "..."});
   *
   * (8) Service Provider PUK depersonalization. Unlocking the service provider
   *     control key (SPCK).
   *
   *   unlockCardLock({lockType: "spckPuk",
   *                   puk: "..."});
   *
   * @return a DOMRequest.
   *         The request's result will be an object containing
   *         information about the unlock operation.
   *
   * Examples:
   *
   * (1) Unlocking failed:
   *
   *     {
   *       lockType:   "pin",
   *       success:    false,
   *       retryCount: 2
   *     }
   *
   * (2) Unlocking succeeded:
   *
   *     {
   *       lockType:  "pin",
   *       success:   true
   *     }
   */
  [Throws]
  nsISupports unlockCardLock(any info);

  /**
   * Modify the state of a card lock.
   *
   * @param info
   *        An object containing information about the lock and
   *        how to modify its state. At a minimum, this object
   *        must have a "lockType" attribute which specifies the
   *        type of lock, e.g. "pin" for the PIN lock. Other
   *        attributes are dependent on the lock type.
   *
   * Examples:
   *
   * (1a) Disabling the PIN lock:
   *
   *   setCardLock({lockType: "pin",
   *                pin: "...",
   *                enabled: false});
   *
   * (1b) Disabling the FDN lock:
   *
   *   setCardLock({lockType: "fdn",
   *                pin2: "...",
   *                enabled: false});
   *
   * (2) Changing the PIN:
   *
   *   setCardLock({lockType: "pin",
   *                pin: "...",
   *                newPin: "..."});
   *
   * @return a DOMRequest.
   *         The request's result will be an object containing
   *         information about the operation.
   *
   * Examples:
   *
   * (1) Enabling/Disabling card lock failed or change card lock failed.
   *
   *     {
   *       lockType: "pin",
   *       success: false,
   *       retryCount: 2
   *     }
   *
   * (2) Enabling/Disabling card lock succeed or change card lock succeed.
   *
   *     {
   *       lockType: "pin",
   *       success: true
   *     }
   */
  [Throws]
  nsISupports setCardLock(any info);

  /**
   * Retrieve the number of remaining tries for unlocking the card.
   *
   * @param lockType
   *        Identifies the lock type, e.g. "pin" for the PIN lock, "puk" for
   *        the PUK lock.
   *
   * @return a DOMRequest.
   *         If the lock type is "pin", or "puk", the request's result will be
   *         an object containing the number of retries for the specified
   *         lock. For any other lock type, the result is undefined.
   */
  [Throws]
  nsISupports getCardLockRetryCount(DOMString lockType);

  // Integrated Circuit Card Phonebook Interfaces.

  /**
   * Read ICC contacts.
   *
   * @param contactType
   *        One of type as below,
   *        - 'adn': Abbreviated Dialling Number.
   *        - 'fdn': Fixed Dialling Number.
   *
   * @return a DOMRequest.
   */
  [Throws]
  nsISupports readContacts(DOMString contactType);

  /**
   * Update ICC Phonebook contact.
   *
   * @param contactType
   *        One of type as below,
   *        - 'adn': Abbreviated Dialling Number.
   *        - 'fdn': Fixed Dialling Number.
   * @param contact
   *        The contact will be updated in ICC.
   * @param [optional] pin2
   *        PIN2 is only required for 'fdn'.
   *
   * @return a DOMRequest.
   */
  [Throws]
  nsISupports updateContact(DOMString contactType,
                           any contact,
                           optional DOMString? pin2 = null);

  // Integrated Circuit Card Secure Element Interfaces.

  /**
   * A secure element is a smart card chip that can hold
   * several different applications with the necessary security.
   * The most known secure element is the Universal Integrated Circuit Card
   * (UICC).
   */

  /**
   * Send request to open a logical channel defined by its
   * application identifier (AID).
   *
   * @param aid
   *        The application identifier of the applet to be selected on this
   *        channel.
   *
   * @return a DOMRequest.
   *         The request's result will be an instance of channel (channelID)
   *         if available or null.
   */
  [Throws]
  nsISupports iccOpenChannel(DOMString aid);

  /**
   * Interface, used to communicate with an applet through the
   * application data protocol units (APDUs) and is
   * used for all data that is exchanged between the UICC and the terminal (ME).
   *
   * @param channel
   *        The application identifier of the applet to which APDU is directed.
   * @param apdu
   *        Application protocol data unit.
   *
   * @return a DOMRequest.
   *         The request's result will be response APDU.
   */
  [Throws]
  nsISupports iccExchangeAPDU(long channel, any apdu);

  /**
   * Send request to close the selected logical channel identified by its
   * application identifier (AID).
   *
   * @param aid
   *        The application identifier of the applet, to be closed.
   *
   * @return a DOMRequest.
   */
  [Throws]
  nsISupports iccCloseChannel(long channel);
};
