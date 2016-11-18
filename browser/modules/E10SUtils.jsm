/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["E10SUtils"];

const {interfaces: Ci, utils: Cu, classes: Cc} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "useRemoteWebExtensions",
                                      "extensions.webextensions.remote", false);

function getAboutModule(aURL) {
  // Needs to match NS_GetAboutModuleName
  let moduleName = aURL.path.replace(/[#?].*/, "").toLowerCase();
  let contract = "@mozilla.org/network/protocol/about;1?what=" + moduleName;
  try {
    return Cc[contract].getService(Ci.nsIAboutModule);
  }
  catch (e) {
    // Either the about module isn't defined or it is broken. In either case
    // ignore it.
    return null;
  }
}

this.E10SUtils = {
  canLoadURIInProcess: function(aURL, aProcess) {
    // loadURI in browser.xml treats null as about:blank
    if (!aURL)
      aURL = "about:blank";

    // Javascript urls can load in any process, they apply to the current document
    if (aURL.startsWith("javascript:"))
      return true;

    let processIsRemote = aProcess == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

    let canLoadRemote = true;
    let mustLoadRemote = true;

    if (aURL.startsWith("about:")) {
      let url = Services.io.newURI(aURL, null, null);
      let module = getAboutModule(url);
      // If the module doesn't exist then an error page will be loading, that
      // should be ok to load in either process
      if (module) {
        let flags = module.getURIFlags(url);
        canLoadRemote = !!(flags & Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD);
        mustLoadRemote = !!(flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD);
      }
    }

    if (aURL.startsWith("chrome:")) {
      let url;
      try {
        // This can fail for invalid Chrome URIs, in which case we will end up
        // not loading anything anyway.
        url = Services.io.newURI(aURL, null, null);
      } catch (ex) {
        canLoadRemote = true;
        mustLoadRemote = false;
      }

      if (url) {
        let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                        getService(Ci.nsIXULChromeRegistry);
        canLoadRemote = chromeReg.canLoadURLRemotely(url);
        mustLoadRemote = chromeReg.mustLoadURLRemotely(url);
      }
    }

    if (aURL.startsWith("moz-extension:")) {
      canLoadRemote = useRemoteWebExtensions;
      mustLoadRemote = useRemoteWebExtensions;
    }

    if (aURL.startsWith("view-source:")) {
      return this.canLoadURIInProcess(aURL.substr("view-source:".length), aProcess);
    }

    if (mustLoadRemote)
      return processIsRemote;

    if (!canLoadRemote && processIsRemote)
      return false;

    return true;
  },

  shouldLoadURI: function(aDocShell, aURI, aReferrer) {
    // Inner frames should always load in the current process
    if (aDocShell.QueryInterface(Ci.nsIDocShellTreeItem).sameTypeParent)
      return true;

    // If the URI can be loaded in the current process then continue
    return this.canLoadURIInProcess(aURI.spec, Services.appinfo.processType);
  },

  redirectLoad: function(aDocShell, aURI, aReferrer, aFreshProcess) {
    // Retarget the load to the correct process
    let messageManager = aDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIContentFrameMessageManager);
    let sessionHistory = aDocShell.getInterface(Ci.nsIWebNavigation).sessionHistory;

    messageManager.sendAsyncMessage("Browser:LoadURI", {
      loadOptions: {
        uri: aURI.spec,
        flags: Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
        referrer: aReferrer ? aReferrer.spec : null,
        reloadInFreshProcess: !!aFreshProcess,
      },
      historyIndex: sessionHistory.requestedIndex,
    });
    return false;
  },

  wrapHandlingUserInput: function(aWindow, aIsHandling, aCallback) {
    var handlingUserInput;
    try {
      handlingUserInput = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils)
                                 .setHandlingUserInput(aIsHandling);
      aCallback();
    } finally {
      handlingUserInput.destruct();
    }
  },
};
