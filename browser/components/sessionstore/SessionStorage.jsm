/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionStorage"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/Console.jsm");

const ssu = Cc["@mozilla.org/browser/sessionstore/utils;1"]
              .createInstance(Ci.nsISessionStoreUtils);

// A bound to the size of data to store for DOM Storage.
const DOM_STORAGE_LIMIT_PREF = "browser.sessionstore.dom_storage_limit";

// Returns the principal for a given |frame| contained in a given |docShell|.
function getPrincipalForFrame(docShell, frame) {
  let ssm = Services.scriptSecurityManager;
  let uri = frame.document.documentURIObject;
  return ssm.getDocShellCodebasePrincipal(uri, docShell);
}

this.SessionStorage = Object.freeze({
  /**
   * Updates all sessionStorage "super cookies"
   * @param docShell
   *        That tab's docshell (containing the sessionStorage)
   * @param frameTree
   *        The docShell's FrameTree instance.
   * @return Returns a nested object that will have hosts as keys and per-origin
   *         session storage data as strings. For example:
   *         {"https://example.com^userContextId=1": {"key": "value", "my_number": "123"}}
   */
  collect(docShell, frameTree) {
    return SessionStorageInternal.collect(docShell, frameTree);
  },

  /**
   * Restores all sessionStorage "super cookies".
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        A nested object with storage data to be restored that has hosts as
   *        keys and per-origin session storage data as strings. For example:
   *        {"https://example.com^userContextId=1": {"key": "value", "my_number": "123"}}
   */
  restore(aDocShell, aStorageData) {
    SessionStorageInternal.restore(aDocShell, aStorageData);
  },
});

/**
 * Calls the given callback |cb|, passing |frame| and each of its descendants.
 */
function forEachNonDynamicChildFrame(frame, cb) {
  // Call for current frame.
  cb(frame);

  // Call the callback recursively for each descendant.
  ssu.forEachNonDynamicChildFrame(frame, subframe => {
    return forEachNonDynamicChildFrame(subframe, cb);
  });
}

var SessionStorageInternal = {
  /**
   * Reads all session storage data from the given docShell.
   * @param content
   *        A tab's global, i.e. the root frame we want to collect for.
   * @return Returns a nested object that will have hosts as keys and per-origin
   *         session storage data as strings. For example:
   *         {"https://example.com^userContextId=1": {"key": "value", "my_number": "123"}}
   */
  collect(content) {
    let data = {};
    let visitedOrigins = new Set();
    let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);

    forEachNonDynamicChildFrame(content, frame => {
      let principal = getPrincipalForFrame(docShell, frame);
      if (!principal) {
        return;
      }

      // Get the origin of the current history entry
      // and use that as a key for the per-principal storage data.
      let origin;
      try {
        // The origin getter may throw for about:blank iframes as of bug 1340710,
        // but we should ignore them anyway.
        origin = principal.origin;
      } catch (e) {
        return;
      }
      if (visitedOrigins.has(origin)) {
        // Don't read a host twice.
        return;
      }

      // Mark the current origin as visited.
      visitedOrigins.add(origin);

      let originData = this._readEntry(principal, docShell);
      if (Object.keys(originData).length) {
        data[origin] = originData;
      }
    });

    return Object.keys(data).length ? data : null;
  },

  /**
   * Writes session storage data to the given tab.
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        A nested object with storage data to be restored that has hosts as
   *        keys and per-origin session storage data as strings. For example:
   *        {"https://example.com^userContextId=1": {"key": "value", "my_number": "123"}}
   */
  restore(aDocShell, aStorageData) {
    for (let origin of Object.keys(aStorageData)) {
      let data = aStorageData[origin];

      let principal;

      try {
        // NOTE: In capture() we record the full origin for the URI which the
        // sessionStorage is being captured for. As of bug 1235657 this code
        // stopped parsing any origins which have originattributes correctly, as
        // it decided to use the origin attributes from the docshell, and try to
        // interpret the origin as a URI. Since bug 1353844 this code now correctly
        // parses the full origin, and then discards the origin attributes, to
        // make the behavior line up with the original intentions in bug 1235657
        // while preserving the ability to read all session storage from
        // previous versions. In the future, if this behavior is desired, we may
        // want to use the spec instead of the origin as the key, and avoid
        // transmitting origin attribute information which we then discard when
        // restoring.
        //
        // If changing this logic, make sure to also change the principal
        // computation logic in SessionStore::_sendRestoreHistory.
        let attrs = aDocShell.getOriginAttributes();
        let dataPrincipal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);
        principal = Services.scriptSecurityManager.createCodebasePrincipal(dataPrincipal.URI, attrs);
      } catch (e) {
        console.error(e);
        continue;
      }

      let storageManager = aDocShell.QueryInterface(Ci.nsIDOMStorageManager);
      let window = aDocShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);

      // There is no need to pass documentURI, it's only used to fill documentURI property of
      // domstorage event, which in this case has no consumer. Prevention of events in case
      // of missing documentURI will be solved in a followup bug to bug 600307.
      let storage = storageManager.createStorage(window, principal, "", aDocShell.usePrivateBrowsing);

      for (let key of Object.keys(data)) {
        try {
          storage.setItem(key, data[key]);
        } catch (e) {
          // throws e.g. for URIs that can't have sessionStorage
          console.error(e);
        }
      }
    }
  },

  /**
   * Reads an entry in the session storage data contained in a tab's history.
   * @param aURI
   *        That history entry uri
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   */
  _readEntry(aPrincipal, aDocShell) {
    let hostData = {};
    let storage;

    let window = aDocShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);

    try {
      let storageManager = aDocShell.QueryInterface(Ci.nsIDOMStorageManager);
      storage = storageManager.getStorage(window, aPrincipal);
      storage.length; // XXX: Bug 1232955 - storage.length can throw, catch that failure
    } catch (e) {
      // sessionStorage might throw if it's turned off, see bug 458954
      storage = null;
    }

    if (!storage || !storage.length) {
      return hostData;
    }

    // If the DOMSessionStorage contains too much data, ignore it.
    let usage = window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils)
                      .getStorageUsage(storage);
    Services.telemetry.getHistogramById("FX_SESSION_RESTORE_DOM_STORAGE_SIZE_ESTIMATE_CHARS").add(usage);
    if (usage > Services.prefs.getIntPref(DOM_STORAGE_LIMIT_PREF)) {
      return hostData;
    }

    for (let i = 0; i < storage.length; i++) {
      try {
        let key = storage.key(i);
        hostData[key] = storage.getItem(key);
      } catch (e) {
        // This currently throws for secured items (cf. bug 442048).
      }
    }

    return hostData;
  }
};
