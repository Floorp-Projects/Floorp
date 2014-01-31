/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Copyright © 2013 Deutsche Telekom, Inc. */

[NoInterfaceObject,
 Func="Navigator::HasNfcManagerSupport"]
interface MozNfcManager {
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
};

[JSImplementation="@mozilla.org/navigatorNfc;1",
 NavigatorProperty="mozNfc",
 Func="Navigator::HasNfcSupport"]
interface MozNfc : EventTarget {
   MozNFCTag getNFCTag(DOMString sessionId);
   MozNFCPeer getNFCPeer(DOMString sessionId);

   [Func="Navigator::HasNfcPeerSupport"]
   attribute EventHandler onpeerready;
   [Func="Navigator::HasNfcPeerSupport"]
   attribute EventHandler onpeerlost;
};

// Mozilla Only
partial interface MozNfc {
   [ChromeOnly]
   void eventListenerWasAdded(DOMString aType);
   [ChromeOnly]
   void eventListenerWasRemoved(DOMString aType);
};

MozNfc implements MozNfcManager;
