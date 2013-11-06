/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Copyright © 2013 Deutsche Telekom, Inc. */

[JSImplementation="@mozilla.org/navigatorNfc;1",
 NavigatorProperty="mozNfc"]
interface MozNfc : EventTarget {
   MozNFCTag getNFCTag(DOMString sessionId);
   MozNFCPeer getNFCPeer(DOMString sessionId);

   /*attribute EventHandler onpeerfound;
   attribute EventHandler onpeerlost;
   attribute EventHandler onforegrounddispatch;*/
};
