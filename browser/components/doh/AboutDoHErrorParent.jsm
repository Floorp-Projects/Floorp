/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutDoHErrorParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "dnsService",
  "@mozilla.org/network/dns-service;1",
  "nsIDNSService"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URLSearchParams"]);

class AboutDoHErrorParent extends JSWindowActorParent {
  receiveMessage({ name }) {
    let params = new URLSearchParams(
      this.browsingContext.currentWindowContext.documentURI.query
    );
    let url = params.get("u");
    if (!url) {
      return;
    }

    switch (name) {
      case "DoH:AllowFallback": {
        // Switch TRR to allow fallback and reload the url.
        Services.prefs.setIntPref("network.trr.mode", 2);
        dnsService.clearCache(true);

        this.browsingContext.top.embedderElement.loadURI(url, {
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
        });
        break;
      }
      case "DoH:Retry": {
        this.browsingContext.top.embedderElement.loadURI(url, {
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
        });
        break;
      }
    }
  }
}
