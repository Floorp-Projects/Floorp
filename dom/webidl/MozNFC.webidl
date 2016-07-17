/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Copyright © 2013 Deutsche Telekom, Inc. */

enum NfcErrorMessage {
  "",
  "IOError",
  "Timeout",
  "Busy",
  "ErrorConnect",
  "ErrorDisconnect",
  "ErrorRead",
  "ErrorWrite",
  "InvalidParameter",
  "InsufficientResource",
  "ErrorSocketCreation",
  "FailEnableDiscovery",
  "FailDisableDiscovery",
  "NotInitialize",
  "InitializeFail",
  "DeinitializeFail",
  "NotSupport",
  "FailEnableLowPowerMode",
  "FailDisableLowPowerMode"
};

[NoInterfaceObject]
interface MozNFCManager {
  /**
   * API to check if the given application's manifest
   * URL is registered with the Chrome Process or not.
   *
   * Returns success if given manifestUrl is registered for 'onpeerready',
   * otherwise error
   */
  [ChromeOnly]
  Promise<boolean> checkP2PRegistration(DOMString manifestUrl);

  /**
   * Notify that user has accepted to share nfc message on P2P UI
   */
  [ChromeOnly]
  void notifyUserAcceptedP2P(DOMString manifestUrl);

  /**
   * Notify the status of sendFile operation
   */
  [ChromeOnly]
  void notifySendFileStatus(octet status, DOMString requestId);

  /**
   * Power on the NFC hardware and start polling for NFC tags or devices.
   */
  [ChromeOnly]
  Promise<void> startPoll();

  /**
   * Stop polling for NFC tags or devices. i.e. enter low power mode.
   */
  [ChromeOnly]
  Promise<void> stopPoll();

  /**
   * Power off the NFC hardware.
   */
  [ChromeOnly]
  Promise<void> powerOff();
};

[JSImplementation="@mozilla.org/nfc/manager;1",
 NavigatorProperty="mozNfc",
 Func="Navigator::HasNFCSupport",
 ChromeOnly,
 UnsafeInPrerendering]
interface MozNFC : EventTarget {
  /**
   * Indicate if NFC is enabled.
   */
  readonly attribute boolean enabled;

  /**
   * This event will be fired when another NFCPeer is detected, and user confirms
   * to share data to the NFCPeer object by calling mozNFC.notifyUserAcceptedP2P.
   * The event will be type of NFCPeerEvent.
   */
  attribute EventHandler onpeerready;

  /**
   * This event will be fired when a NFCPeer is detected. The application has to
   * be running on the foreground (decided by System app) to receive this event.
   *
   * The default action of this event is to dispatch the event in System app
   * again, and System app will run the default UX behavior (like vibration).
   * So if the application would like to cancel the event, the application
   * should call event.preventDefault() or return false in this event handler.
   */
  attribute EventHandler onpeerfound;

  /**
   * This event will be fired when NFCPeer, earlier detected in onpeerready
   * or onpeerfound, moves out of range, or if the application has been switched
   * to the background (decided by System app).
   */
  attribute EventHandler onpeerlost;

  /**
   * This event will be fired when a NFCTag is detected. The application has to
   * be running on the foreground (decided by System app) to receive this event.
   *
   * The default action of this event is to dispatch the event in System app
   * again, and System app will run the default UX behavior (like vibration) and
   * launch MozActivity to handle the content of the tag. (For example, System
   * app will launch Browser if the tag contains URL). So if the application
   * would like to cancel the event, i.e. in the above example, the application
   * would process the URL by itself without launching Browser, the application
   * should call event.preventDefault() or return false in this event handler.
   */
  attribute EventHandler ontagfound;

  /**
   * This event will be fired if the tag detected in ontagfound has been
   * removed, or if the application has been switched to the background (decided
   * by System app).
   */
  attribute EventHandler ontaglost;
};

// Mozilla Only
partial interface MozNFC {
  [ChromeOnly]
  void eventListenerWasAdded(DOMString aType);
  [ChromeOnly]
  void eventListenerWasRemoved(DOMString aType);
};

MozNFC implements MozNFCManager;
