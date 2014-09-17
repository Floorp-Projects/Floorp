/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Copyright © 2013 Deutsche Telekom, Inc. */

[NoInterfaceObject]
interface MozNFCManager {
   /**
    * API to check if the given application's manifest
    * URL is registered with the Chrome Process or not.
    *
    * Returns success if given manifestUrl is registered for 'onpeerready',
    * otherwise error
    */
   [CheckPermissions="nfc-manager"]
   DOMRequest checkP2PRegistration(DOMString manifestUrl);

   /**
    * Notify that user has accepted to share nfc message on P2P UI
    */
   [CheckPermissions="nfc-manager"]
   void notifyUserAcceptedP2P(DOMString manifestUrl);

   /**
    * Notify the status of sendFile operation
    */
   [CheckPermissions="nfc-manager"]
   void notifySendFileStatus(octet status, DOMString requestId);

   /**
    * Power on the NFC hardware and start polling for NFC tags or devices.
    */
   [CheckPermissions="nfc-manager"]
   DOMRequest startPoll();

   /**
    * Stop polling for NFC tags or devices. i.e. enter low power mode.
    */
   [CheckPermissions="nfc-manager"]
   DOMRequest stopPoll();

   /**
    * Power off the NFC hardware.
    */
   [CheckPermissions="nfc-manager"]
   DOMRequest powerOff();
};

[JSImplementation="@mozilla.org/navigatorNfc;1",
 NavigatorProperty="mozNfc",
 Func="Navigator::HasNFCSupport",
 CheckPermissions="nfc-read nfc-write",
 AvailableIn="CertifiedApps"]
interface MozNFC : EventTarget {
   /**
    * Returns MozNFCTag object or null in case of invalid sessionToken
    */
   MozNFCTag? getNFCTag(DOMString sessionToken);

   /**
    * Returns MozNFCPeer object or null in case of invalid sessionToken
    */
   MozNFCPeer? getNFCPeer(DOMString sessionToken);

   /**
    * This event will be fired when another NFCPeer is detected, and user confirms
    * to share data to the NFCPeer object by calling mozNFC.notifyUserAcceptedP2P.
    * The event will be type of NFCPeerEvent.
    */
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
