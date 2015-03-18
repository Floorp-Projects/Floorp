/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum IccCardState
{
  "unknown", // ICC card state is either not yet reported from modem or in an
             // unknown state.
  "ready",
  "pinRequired",
  "pukRequired",
  "permanentBlocked",

  /**
   * Personalization States
   */
  "personalizationInProgress",
  "personalizationReady",

  // SIM Personalization States.
  "networkLocked",
  "networkSubsetLocked",
  "corporateLocked",
  "serviceProviderLocked",
  "simPersonalizationLocked",
  "networkPukRequired",
  "networkSubsetPukRequired",
  "corporatePukRequired",
  "serviceProviderPukRequired",
  "simPersonalizationPukRequired",

  // RUIM Personalization States.
  "network1Locked",
  "network2Locked",
  "hrpdNetworkLocked",
  "ruimCorporateLocked",
  "ruimServiceProviderLocked",
  "ruimPersonalizationLocked",
  "network1PukRequired",
  "network2PukRequired",
  "hrpdNetworkPukRequired",
  "ruimCorporatePukRequired",
  "ruimServiceProviderPukRequired",
  "ruimPersonalizationPukRequired",

  /**
   * Additional States.
   */
  "illegal" // See Bug 916000. An owed pay card will be rejected by the network
            // and fall in this state.
};

enum IccLockType
{
  "pin",
  "pin2",
  "puk",
  "puk2",
  "nck",      // Network depersonalization -- network control key (NCK).
  "nsck",     // Network subset depersonalization -- network subset control key (NSCK).
  "nck1",     // Network type 1 depersonalization -- network type 1 control key (NCK1).
  "nck2",     // Network type 2 depersonalization -- network type 2 control key (NCK2).
  "hnck",     // HRPD network depersonalization -- HRPD network control key (HNCK).
  "cck",      // Corporate depersonalization -- corporate control key (CCK).
  "spck",     // Service provider depersonalization -- service provider control key (SPCK).
  "pck",      // SIM depersonalization -- personalization control key (PCK).
  "rcck",     // RUIM corporate depersonalization -- RUIM corporate control key (RCCK).
  "rspck",    // RUIM service provider depersonalization -- RUIM service provider control key (RSPCK).
  "nckPuk",   // Network PUK depersonalization -- network control key (NCK).
  "nsckPuk",  // Network subset PUK depersonalization -- network subset control key (NSCK).
  "nck1Puk",  // Network type 1 PUK depersonalization -- network type 1 control key (NCK1).
  "nck2Puk",  // Network type 2 PUK depersonalization -- Network type 2 control key (NCK2).
  "hnckPuk",  // HRPD network PUK depersonalization -- HRPD network control key (HNCK).
  "cckPuk",   // Corporate PUK depersonalization -- corporate control key (CCK).
  "spckPuk",  // Service provider PUK depersonalization -- service provider control key (SPCK).
  "pckPuk",   // SIM PUK depersonalization -- personalization control key (PCK).
  "rcckPuk",  // RUIM corporate PUK depersonalization -- RUIM corporate control key (RCCK).
  "rspckPuk", // RUIM service provider PUK depersonalization -- service provider control key (SPCK).
  "fdn"
};

enum IccContactType
{
  "adn", // Abbreviated Dialling Number.
  "fdn", // Fixed Dialling Number.
  "sdn"  // Service Dialling Number.
};

enum IccMvnoType
{
  "imsi",
  "spn",
  "gid"
};

enum IccService
{
  "fdn"
};

dictionary IccUnlockCardLockOptions
{
  required IccLockType lockType;

  DOMString? pin = null; // Necessary for lock types: "pin", "pin2", "nck",
                         // "nsck", "nck1", "nck2", "hnck", "cck", "spck",
                         // "pck", "rcck", "rspck".

  DOMString? puk = null; // Necessary for lock types: "puk", "puk2", "nckPuk",
                         // "nsckPuk", "nck1Puk", "nck2Puk", "hnckPuk", "cckPuk",
                         // "spckPuk", "pckPuk", "rcckPuk", "rspckPuk".

  DOMString? newPin = null; // Necessary for lock types: "puk", "puk2".
};

dictionary IccSetCardLockOptions
{
 required IccLockType lockType;

 DOMString? pin = null; // Necessary for lock types: "pin", "pin2"

 DOMString? pin2 = null; // Used for enabling/disabling operation.
                         // Necessary for lock types: "fdn".

 DOMString? newPin = null; // Used for changing password operation.
                           // Necessary for lock types: "pin", "pin2"

 boolean enabled; // Used for enabling/disabling operation.
                  // Necessary for lock types: "pin", "fdn"
};

[Pref="dom.icc.enabled",
 CheckPermissions="mobileconnection",
 AvailableIn="CertifiedApps"]
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
  readonly attribute (MozIccInfo or MozGsmIccInfo or MozCdmaIccInfo)? iccInfo;

  /**
   * The 'iccinfochange' event is notified whenever the icc info object
   * changes.
   */
  attribute EventHandler oniccinfochange;

  // Integrated Circuit Card State.

  /**
   * Indicates the state of the device's ICC.
   *
   * Once the ICC becomes undetectable, cardstatechange event will be notified.
   * Also, the attribute is set to null and this MozIcc object becomes invalid.
   * Calling asynchronous functions raises exception then.
   */
  readonly attribute IccCardState? cardState;

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
   *        Identifies the lock type.
   *
   * @return a DOMRequest.
   *         The request's result will be an object containing
   *         information about the specified lock's status.
   *         e.g. {enabled: true}.
   */
  [Throws]
  DOMRequest getCardLock(IccLockType lockType);

  /**
   * Unlock a card lock.
   *
   * @param info
   *        An object containing the information necessary to unlock
   *        the given lock.
   *
   * @return a DOMRequest.
   *         The request's error will be an object containing the number of
   *         remaining retries
   *         @see IccCardLockError.
   */
  [Throws]
  DOMRequest unlockCardLock(optional IccUnlockCardLockOptions info);

  /**
   * Modify the state of a card lock.
   *
   * @param info
   *        An object containing information about the lock and
   *        how to modify its state.
   *
   * @return a DOMRequest.
   *         The request's error will be an object containing the number of
   *         remaining retries.
   *         @see IccCardLockError.
   */
  [Throws]
  DOMRequest setCardLock(optional IccSetCardLockOptions info);

  /**
   * Retrieve the number of remaining tries for unlocking the card.
   *
   * @param lockType
   *        Identifies the lock type.
   *
   * @return a DOMRequest.
   *         The request's result will be an object containing the number of
   *         remaining retries.
   *         e.g. {retryCount: 3}.
   */
  [Throws]
  DOMRequest getCardLockRetryCount(IccLockType lockType);

  // Integrated Circuit Card Phonebook Interfaces.

  /**
   * Read ICC contacts.
   *
   * @param contactType
   *        Identifies the contact type.
   *
   * @return a DOMRequest.
   */
  [Throws]
  DOMRequest readContacts(IccContactType contactType);

  /**
   * Update ICC Phonebook contact.
   *
   * @param contactType
   *        Identifies the contact type.
   * @param contact
   *        The contact will be updated in ICC.
   * @param pin2 [optional]
   *        PIN2 is only required for "fdn".
   *
   * @return a DOMRequest.
   */
  [Throws]
  DOMRequest updateContact(IccContactType contactType,
                           any contact,
                           optional DOMString? pin2 = null);

  // Integrated Circuit Card Helpers.

  /**
   * Verify whether the passed data (matchData) matches with some ICC's field
   * according to the mvno type (mvnoType).
   *
   * @param mvnoType
   *        Mvno type to use to compare the match data.
   * @param matchData
   *        Data to be compared with ICC's field.
   *
   * @return a DOMRequest.
   *         The request's result will be a boolean indicating the matching
   *         result.
   */
  [Throws]
  DOMRequest matchMvno(IccMvnoType mvnoType, DOMString matchData);

  /**
   * Retrieve the the availability of an icc service.
   *
   * @param service
   *        Identifies the service type.
   *
   * @return a Promise
   *         If succeeds, the promise is resolved with boolean indicating the
   *         availability of the service. Otherwise, rejected with a DOMError.
   */
  [NewObject]
  Promise<boolean> getServiceState(IccService service);
};
