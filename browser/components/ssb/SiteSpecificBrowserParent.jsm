/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SiteSpecificBrowserParent"];

const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

class SiteSpecificBrowserParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "RetargetOutOfScopeURIToBrowser":
        // The content process found a URI that needs to be loaded in the main
        // browser.
        let triggeringPrincipal = E10SUtils.deserializePrincipal(
          message.data.triggeringPrincipal
        );
        let referrerInfo = E10SUtils.deserializeReferrerInfo(
          message.data.referrerInfo
        );
        let csp = E10SUtils.deserializeCSP(message.data.csp);

        // Attempt to find an existing window to open it in.
        let win = BrowserWindowTracker.getTopWindow();
        if (win) {
          win.gBrowser.selectedTab = win.gBrowser.addTab(message.data.uri, {
            triggeringPrincipal,
            csp,
            referrerInfo,
          });
        } else {
          let sa = Cc["@mozilla.org/array;1"].createInstance(
            Ci.nsIMutableArray
          );

          let wuri = Cc["@mozilla.org/supports-string;1"].createInstance(
            Ci.nsISupportsString
          );
          wuri.data = message.data.uri;

          sa.appendElement(wuri);
          sa.appendElement(null); // unused (bug 871161)
          sa.appendElement(referrerInfo);
          sa.appendElement(null); // postData
          sa.appendElement(null); // allowThirdPartyFixup
          sa.appendElement(null); // userContextId
          sa.appendElement(null); // originPrincipal
          sa.appendElement(null); // originStoragePrincipal
          sa.appendElement(triggeringPrincipal);
          sa.appendElement(null); // allowInheritPrincipal
          sa.appendElement(csp);

          Services.ww.openWindow(
            null,
            AppConstants.BROWSER_CHROME_URL,
            null,
            "chrome,dialog=no,all",
            sa
          );
        }
        break;
    }
  }
}
