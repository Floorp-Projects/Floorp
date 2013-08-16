/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionStorage"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");

this.SessionStorage = {
  /**
   * Updates all sessionStorage "super cookies"
   * @param aDocShell
   *        That tab's docshell (containing the sessionStorage)
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   */
  serialize: function ssto_serialize(aDocShell, aFullData) {
    return DomStorage.read(aDocShell, aFullData);
  },

  /**
   * Restores all sessionStorage "super cookies".
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        Storage data to be restored
   */
  deserialize: function ssto_deserialize(aDocShell, aStorageData) {
    DomStorage.write(aDocShell, aStorageData);
  }
};

Object.freeze(SessionStorage);

let DomStorage = {
  /**
   * Reads all session storage data from the given docShell.
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aFullData
   *        Always return privacy sensitive data (use with care)
   */
  read: function DomStorage_read(aDocShell, aFullData) {
    let data = {};
    let isPinned = aDocShell.isAppTab;
    let shistory = aDocShell.sessionHistory;

    for (let i = 0; i < shistory.count; i++) {
      let principal = History.getPrincipalForEntry(shistory, i, aDocShell);
      if (!principal)
        continue;

      // Check if we're allowed to store sessionStorage data.
      let isHTTPS = principal.URI && principal.URI.schemeIs("https");
      if (aFullData || SessionStore.checkPrivacyLevel(isHTTPS, isPinned)) {
        let origin = principal.extendedOrigin;

        // Don't read a host twice.
        if (!(origin in data)) {
          let originData = this._readEntry(principal, aDocShell);
          if (Object.keys(originData).length) {
            data[origin] = originData;
          }
        }
      }
    }

    return data;
  },

  /**
   * Writes session storage data to the given tab.
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        Storage data to be restored
   */
  write: function DomStorage_write(aDocShell, aStorageData) {
    for (let [host, data] in Iterator(aStorageData)) {
      let uri = Services.io.newURI(host, null, null);
      let principal = Services.scriptSecurityManager.getDocShellCodebasePrincipal(uri, aDocShell);
      let storageManager = aDocShell.QueryInterface(Components.interfaces.nsIDOMStorageManager);

      // There is no need to pass documentURI, it's only used to fill documentURI property of
			// domstorage event, which in this case has no consumer.  Prevention of events in case
			// of missing documentURI will be solved in a followup bug to bug 600307.
      let storage = storageManager.createStorage(principal, "", aDocShell.usePrivateBrowsing);

      for (let [key, value] in Iterator(data)) {
        try {
          storage.setItem(key, value);
        } catch (e) {
          // throws e.g. for URIs that can't have sessionStorage
          Cu.reportError(e);
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
  _readEntry: function DomStorage_readEntry(aPrincipal, aDocShell) {
    let hostData = {};
    let storage;

    try {
      let storageManager = aDocShell.QueryInterface(Components.interfaces.nsIDOMStorageManager);
      storage = storageManager.getStorage(aPrincipal);
    } catch (e) {
      // sessionStorage might throw if it's turned off, see bug 458954
    }

    if (storage && storage.length) {
       for (let i = 0; i < storage.length; i++) {
        try {
          let key = storage.key(i);
          hostData[key] = storage.getItem(key);
        } catch (e) {
          // This currently throws for secured items (cf. bug 442048).
        }
      }
    }

    return hostData;
  }
};

let History = {
  /**
   * Returns a given history entry's URI.
   * @param aHistory
   *        That tab's session history
   * @param aIndex
   *        The history entry's index
   * @param aDocShell
   *        That tab's docshell
   */
  getPrincipalForEntry: function History_getPrincipalForEntry(aHistory,
                                                              aIndex,
                                                              aDocShell) {
    try {
      return Services.scriptSecurityManager.getDocShellCodebasePrincipal(
        aHistory.getEntryAtIndex(aIndex, false).URI, aDocShell);
    } catch (e) {
      // This might throw for some reason.
    }
  },
};
