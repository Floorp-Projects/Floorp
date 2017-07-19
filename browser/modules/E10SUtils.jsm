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
XPCOMUtils.defineLazyPreferenceGetter(this, "useSeparateFileUriProcess",
                                      "browser.tabs.remote.separateFileUriProcess", false);
XPCOMUtils.defineLazyPreferenceGetter(this, "allowLinkedWebInFileUriProcess",
                                      "browser.tabs.remote.allowLinkedWebInFileUriProcess", false);
XPCOMUtils.defineLazyModuleGetter(this, "Utils",
                                  "resource://gre/modules/sessionstore/Utils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/Console.jsm");

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

// This must start with the WEB_REMOTE_TYPE above.
const LARGE_ALLOCATION_REMOTE_TYPE = "webLargeAllocation";
const DEFAULT_REMOTE_TYPE = WEB_REMOTE_TYPE;

function validatedWebRemoteType(aPreferredRemoteType, aTargetUri, aCurrentUri) {
  // If the domain is whitelisted to allow it to use file:// URIs, then we have
  // to run it in a file content process, in case it uses file:// sub-resources.
  const sm = Services.scriptSecurityManager;
  if (sm.inFileURIWhitelist(aTargetUri)) {
    return FILE_REMOTE_TYPE;
  }

  if (!aPreferredRemoteType) {
    return WEB_REMOTE_TYPE;
  }

  if (aPreferredRemoteType.startsWith(WEB_REMOTE_TYPE)) {
    return aPreferredRemoteType;
  }

  if (allowLinkedWebInFileUriProcess &&
      aPreferredRemoteType == FILE_REMOTE_TYPE) {
    // If aCurrentUri is passed then we should only allow FILE_REMOTE_TYPE
    // when it is same origin as target.
    if (aCurrentUri) {
      try {
        // checkSameOriginURI throws when not same origin.
        sm.checkSameOriginURI(aCurrentUri, aTargetUri, false);
        return FILE_REMOTE_TYPE;
      } catch (e) {
        return WEB_REMOTE_TYPE;
      }
    }

    return FILE_REMOTE_TYPE;
  }

  return WEB_REMOTE_TYPE;
}

this.E10SUtils = {
  DEFAULT_REMOTE_TYPE,
  NOT_REMOTE,
  WEB_REMOTE_TYPE,
  FILE_REMOTE_TYPE,
  EXTENSION_REMOTE_TYPE,
  LARGE_ALLOCATION_REMOTE_TYPE,

  canLoadURIInProcess(aURL, aProcess) {
    let remoteType = aProcess == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT
                     ? DEFAULT_REMOTE_TYPE : NOT_REMOTE;
    return remoteType == this.getRemoteTypeForURI(aURL, true, remoteType);
  },

  getRemoteTypeForURI(aURL, aMultiProcess,
                      aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
                      aCurrentUri) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    // loadURI in browser.xml treats null as about:blank
    if (!aURL) {
      aURL = "about:blank";
    }

    let uri;
    try {
      uri = Services.uriFixup.createFixupURI(aURL, Ci.nsIURIFixup.FIXUP_FLAG_NONE);
    } catch (e) {
      // If we have an invalid URI, it's still possible that it might get
      // fixed-up into a valid URI later on. However, we don't want to return
      // aPreferredRemoteType here, in case the URI gets fixed-up into
      // something that wouldn't normally run in that process.
      return DEFAULT_REMOTE_TYPE;
    }

    return this.getRemoteTypeForURIObject(uri, aMultiProcess,
                                          aPreferredRemoteType, aCurrentUri);
  },

  getRemoteTypeForURIObject(aURI, aMultiProcess,
                            aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
                            aCurrentUri) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    switch (aURI.scheme) {
      case "javascript":
        // javascript URIs can load in any, they apply to the current document.
        return aPreferredRemoteType;

      case "data":
      case "blob":
        // We need data: and blob: URIs to load in any remote process, because
        // they need to be able to load in whatever is the current process
        // unless it is non-remote. In that case we don't want to load them in
        // the parent process, so we load them in the default remote process,
        // which is sandboxed and limits any risk.
        return aPreferredRemoteType == NOT_REMOTE ? DEFAULT_REMOTE_TYPE
                                                  : aPreferredRemoteType;

      case "file":
        return useSeparateFileUriProcess ? FILE_REMOTE_TYPE
                                         : DEFAULT_REMOTE_TYPE;

      case "about":
        let module = getAboutModule(aURI);
        // If the module doesn't exist then an error page will be loading, that
        // should be ok to load in any process
        if (!module) {
          return aPreferredRemoteType;
        }

        let flags = module.getURIFlags(aURI);
        if (flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD) {
          return DEFAULT_REMOTE_TYPE;
        }

        // If the about page can load in parent or child, it should be safe to
        // load in any remote type.
        if (flags & Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD) {
          return aPreferredRemoteType;
        }

        return NOT_REMOTE;

      case "chrome":
        let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                        getService(Ci.nsIXULChromeRegistry);
        if (chromeReg.mustLoadURLRemotely(aURI)) {
          return DEFAULT_REMOTE_TYPE;
        }

        if (chromeReg.canLoadURLRemotely(aURI) &&
            aPreferredRemoteType != NOT_REMOTE) {
          return DEFAULT_REMOTE_TYPE;
        }

        return NOT_REMOTE;

      case "moz-extension":
        return useRemoteWebExtensions ? EXTENSION_REMOTE_TYPE : NOT_REMOTE;

      default:
        // For any other nested URIs, we use the innerURI to determine the
        // remote type. In theory we should use the innermost URI, but some URIs
        // have fake inner URIs (e.g. about URIs with inner moz-safe-about) and
        // if such URIs are wrapped in other nested schemes like view-source:,
        // we don't want to "skip" past "about:" by going straight to the
        // innermost URI. Any URIs like this will need to be handled in the
        // cases above, so we don't still end up using the fake inner URI here.
        if (aURI instanceof Ci.nsINestedURI) {
          let innerURI = aURI.QueryInterface(Ci.nsINestedURI).innerURI;
          return this.getRemoteTypeForURIObject(innerURI, aMultiProcess,
                                                aPreferredRemoteType,
                                                aCurrentUri);
        }

        return validatedWebRemoteType(aPreferredRemoteType, aURI, aCurrentUri);
    }
  },

  shouldLoadURIInThisProcess(aURI) {
    let remoteType = Services.appinfo.remoteType;
    return remoteType == this.getRemoteTypeForURIObject(aURI, true, remoteType);
  },

  shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData) {
    // Inner frames should always load in the current process
    if (aDocShell.QueryInterface(Ci.nsIDocShellTreeItem).sameTypeParent)
      return true;

    // If we are in a Large-Allocation process, and it wouldn't be content visible
    // to change processes, we want to load into a new process so that we can throw
    // this one out. We don't want to move into a new process if we have post data,
    // because we would accidentally throw out that data.
    if (!aHasPostData &&
        Services.appinfo.remoteType == LARGE_ALLOCATION_REMOTE_TYPE &&
        !aDocShell.awaitingLargeAlloc &&
        aDocShell.isOnlyToplevelInTabGroup) {
      return false;
    }

    // Allow history load if loaded in this process before.
    let webNav = aDocShell.QueryInterface(Ci.nsIWebNavigation);
    let sessionHistory = webNav.sessionHistory;
    let requestedIndex = sessionHistory.requestedIndex;
    if (requestedIndex >= 0) {
      if (sessionHistory.getEntryAtIndex(requestedIndex, false).loadedInThisProcess) {
        return true;
      }

      // If not originally loaded in this process allow it if the URI would
      // normally be allowed to load in this process by default.
      let remoteType = Services.appinfo.remoteType;
      return remoteType ==
        this.getRemoteTypeForURIObject(aURI, true, remoteType, webNav.currentURI);
    }

    // If the URI can be loaded in the current process then continue
    return this.shouldLoadURIInThisProcess(aURI);
  },

  redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, aFreshProcess, aFlags) {
    // Retarget the load to the correct process
    let messageManager = aDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIContentFrameMessageManager);
    let sessionHistory = aDocShell.getInterface(Ci.nsIWebNavigation).sessionHistory;

    messageManager.sendAsyncMessage("Browser:LoadURI", {
      loadOptions: {
        uri: aURI.spec,
        flags: aFlags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
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
