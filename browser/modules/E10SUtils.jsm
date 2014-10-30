/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["E10SUtils"];

const {interfaces: Ci, utils: Cu, classes: Cc} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.E10SUtils = {
  shouldBrowserBeRemote: function(aURL) {
    // loadURI in browser.xml treats null as about:blank
    if (!aURL)
      aURL = "about:blank";

    if (aURL.startsWith("about:") &&
        aURL.toLowerCase() != "about:home" &&
        aURL.toLowerCase() != "about:blank" &&
        !aURL.toLowerCase().startsWith("about:neterror") &&
        !aURL.toLowerCase().startsWith("about:certerror")) {
      return false;
    }

    return true;
  },

  shouldLoadURI: function(aDocShell, aURI, aReferrer) {
    // about:blank is the initial document and can load anywhere
    if (aURI.spec == "about:blank")
      return true;

    // Inner frames should always load in the current process
    if (aDocShell.QueryInterface(Ci.nsIDocShellTreeItem).sameTypeParent)
      return true;

    // If the URI can be loaded in the current process then continue
    let isRemote = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;
    if (this.shouldBrowserBeRemote(aURI.spec) == isRemote)
      return true;

    return false;
  },

  redirectLoad: function(aDocShell, aURI, aReferrer) {
    // Retarget the load to the correct process
    let messageManager = aDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIContentFrameMessageManager);
    let sessionHistory = aDocShell.getInterface(Ci.nsIWebNavigation).sessionHistory;

    messageManager.sendAsyncMessage("Browser:LoadURI", {
      loadOptions: {
        uri: aURI.spec,
        flags: Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
        referrer: aReferrer ? aReferrer.spec : null,
      },
      historyIndex: sessionHistory.requestedIndex,
    });
    return false;
  },
};
