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
XPCOMUtils.defineLazyModuleGetter(this, "Utils",
                                  "resource://gre/modules/sessionstore/Utils.jsm");

function getAboutModule(aURL) {
  // Needs to match NS_GetAboutModuleName
  let moduleName = aURL.path.replace(/[#?].*/, "").toLowerCase();
  let contract = "@mozilla.org/network/protocol/about;1?what=" + moduleName;
  try {
    return Cc[contract].getService(Ci.nsIAboutModule);
  } catch (e) {
    // Either the about module isn't defined or it is broken. In either case
    // ignore it.
    return null;
  }
}

const NOT_REMOTE = null;

// These must match any similar ones in ContentParent.h.
const WEB_REMOTE_TYPE = "web";
const FILE_REMOTE_TYPE = "file";
const EXTENSION_REMOTE_TYPE = "extension";
const DEFAULT_REMOTE_TYPE = WEB_REMOTE_TYPE;

function validatedWebRemoteType(aPreferredRemoteType) {
  return aPreferredRemoteType && aPreferredRemoteType.startsWith(WEB_REMOTE_TYPE)
         ? aPreferredRemoteType : WEB_REMOTE_TYPE;
}

this.E10SUtils = {
  DEFAULT_REMOTE_TYPE,
  NOT_REMOTE,
  WEB_REMOTE_TYPE,
  FILE_REMOTE_TYPE,
  EXTENSION_REMOTE_TYPE,

  canLoadURIInProcess(aURL, aProcess) {
    let remoteType = aProcess == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT
                     ? DEFAULT_REMOTE_TYPE : NOT_REMOTE;
    return remoteType == this.getRemoteTypeForURI(aURL, true, remoteType);
  },

  getRemoteTypeForURI(aURL, aMultiProcess,
                                aPreferredRemoteType = DEFAULT_REMOTE_TYPE) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    // loadURI in browser.xml treats null as about:blank
    if (!aURL) {
      aURL = "about:blank";
    }

    // Javascript urls can load in any process, they apply to the current document
    if (aURL.startsWith("javascript:")) {
      return aPreferredRemoteType;
    }

    // We need data: URIs to load in any remote process, because some of our
    // tests rely on this.
    if (aURL.startsWith("data:")) {
      return aPreferredRemoteType == NOT_REMOTE ? DEFAULT_REMOTE_TYPE
                                                : aPreferredRemoteType;
    }

    if (aURL.startsWith("file:")) {
      return Services.prefs.getBoolPref("browser.tabs.remote.separateFileUriProcess")
             ? FILE_REMOTE_TYPE : DEFAULT_REMOTE_TYPE;
    }

    if (aURL.startsWith("about:")) {
      // We need to special case about:blank because it needs to load in any.
      if (aURL == "about:blank") {
        return aPreferredRemoteType;
      }

      let url = Services.io.newURI(aURL);
      let module = getAboutModule(url);
      // If the module doesn't exist then an error page will be loading, that
      // should be ok to load in any process
      if (!module) {
        return aPreferredRemoteType;
      }

      let flags = module.getURIFlags(url);
      if (flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD) {
        return DEFAULT_REMOTE_TYPE;
      }

      // If the about page can load in parent or child, it should be safe to
      // load in any remote type.
      if (flags & Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD) {
        return aPreferredRemoteType;
      }

      return NOT_REMOTE;
    }

    if (aURL.startsWith("chrome:")) {
      let url;
      try {
        // This can fail for invalid Chrome URIs, in which case we will end up
        // not loading anything anyway.
        url = Services.io.newURI(aURL);
      } catch (ex) {
        return aPreferredRemoteType;
      }

      let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                      getService(Ci.nsIXULChromeRegistry);
      if (chromeReg.mustLoadURLRemotely(url)) {
        return DEFAULT_REMOTE_TYPE;
      }

      if (chromeReg.canLoadURLRemotely(url) &&
          aPreferredRemoteType != NOT_REMOTE) {
        return DEFAULT_REMOTE_TYPE;
      }

      return NOT_REMOTE;
    }

    if (aURL.startsWith("moz-extension:")) {
      return useRemoteWebExtensions ? EXTENSION_REMOTE_TYPE : NOT_REMOTE;
    }

    if (aURL.startsWith("view-source:")) {
      return this.getRemoteTypeForURI(aURL.substr("view-source:".length),
                                      aMultiProcess, aPreferredRemoteType);
    }

    return validatedWebRemoteType(aPreferredRemoteType);
  },

  shouldLoadURIInThisProcess(aURI) {
    let remoteType = Services.appinfo.remoteType;
    return remoteType == this.getRemoteTypeForURI(aURI.spec, true, remoteType);
  },

  shouldLoadURI(aDocShell, aURI, aReferrer) {
    // Inner frames should always load in the current process
    if (aDocShell.QueryInterface(Ci.nsIDocShellTreeItem).sameTypeParent)
      return true;

    // If we are in a fresh process, and it wouldn't be content visible to
    // change processes, we want to load into a new process so that we can throw
    // this one out.
    if (aDocShell.inFreshProcess && aDocShell.isOnlyToplevelInTabGroup) {
      return false;
    }

    // If the URI can be loaded in the current process then continue
    return this.shouldLoadURIInThisProcess(aURI);
  },

  redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, aFreshProcess) {
    // Retarget the load to the correct process
    let messageManager = aDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIContentFrameMessageManager);
    let sessionHistory = aDocShell.getInterface(Ci.nsIWebNavigation).sessionHistory;

    messageManager.sendAsyncMessage("Browser:LoadURI", {
      loadOptions: {
        uri: aURI.spec,
        flags: Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
        referrer: aReferrer ? aReferrer.spec : null,
        triggeringPrincipal: aTriggeringPrincipal
                             ? Utils.serializePrincipal(aTriggeringPrincipal)
                             : null,
        reloadInFreshProcess: !!aFreshProcess,
      },
      historyIndex: sessionHistory.requestedIndex,
    });
    return false;
  },

  wrapHandlingUserInput(aWindow, aIsHandling, aCallback) {
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
