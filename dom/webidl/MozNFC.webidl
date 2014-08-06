/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Copyright © 2013 Deutsche Telekom, Inc. */

[NoInterfaceObject,
 CheckPermissions="nfc-manager"]
interface MozNFCManager {
   /**
    * API to check if the given application's manifest
    * URL is registered with the Chrome Process or not.
    *
    * Returns success if given manifestUrl is registered for 'onpeerready',
    * otherwise error
    */
   DOMRequest checkP2PRegistration(DOMString manifestUrl);

   /**
    * Notify that user has accepted to share nfc message on P2P UI
    */
   void notifyUserAcceptedP2P(DOMString manifestUrl);

   /**
    * Notify the status of sendFile operation
    */
   void notifySendFileStatus(octet status, DOMString requestId);

   /**
    * Power on the NFC hardware and start polling for NFC tags or devices.
    */
   DOMRequest startPoll();

   /**
    * Stop polling for NFC tags or devices. i.e. enter low power mode.
    */
   DOMRequest stopPoll();

   /**
    * Power off the NFC hardware.
    */
   DOMRequest powerOff();
};

[JSImplementation="@mozilla.org/navigatorNfc;1",
 NavigatorProperty="mozNfc",
 Func="Navigator::HasNFCSupport"]
interface MozNFC : EventTarget {
   [Throws]
   MozNFCTag getNFCTag(DOMString sessionId);
   [Throws]
   MozNFCPeer getNFCPeer(DOMString sessionId);

   [CheckPermissions="nfc-write"]
   attribute EventHandler onpeerready;
   [CheckPermissions="nfc-write"]
   attribute EventHandler onpeerlost;
};

// Mozilla Only
partial interface MozNFC {
   [ChromeOnly]
   void eventListenerWasAdded(DOMString aType);
   [ChromeOnly]
   void eventListenerWasRemoved(DOMString aType);
};

MozNFC implements MozNFCManager;
