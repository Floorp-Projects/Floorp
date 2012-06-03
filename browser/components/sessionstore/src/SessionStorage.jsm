/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["SessionStorage"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");

let SessionStorage = {
  /**
   * Updates all sessionStorage "super cookies"
   * @param aTabData
   *        The data object for a specific tab
   * @param aHistory
   *        That tab's session history
   * @param aDocShell
   *        That tab's docshell (containing the sessionStorage)
   * @param aFullData
   *        always return privacy sensitive data (use with care)
   * @param aIsPinned
   *        the tab is pinned and should be treated differently for privacy
   */
  serialize: function ssto_serialize(aTabData, aHistory, aDocShell, aFullData,
                                     aIsPinned) {
    let storageData = {};
    let hasContent = false;

    for (let i = 0; i < aHistory.count; i++) {
      let uri;
      try {
        uri = aHistory.getEntryAtIndex(i, false).URI;
      }
      catch (ex) {
        // Chances are that this is getEntryAtIndex throwing, as seen in bug 669196.
        // We've already asserted in _collectTabData, so we won't show that again.
        continue;
      }
      // sessionStorage is saved per origin (cf. nsDocShell::GetSessionStorageForURI)
      let domain = uri.spec;
      try {
        if (uri.host)
          domain = uri.prePath;
      }
      catch (ex) { /* this throws for host-less URIs (such as about: or jar:) */ }
      if (storageData[domain] ||
          !(aFullData || SessionStore.checkPrivacyLevel(uri.schemeIs("https"), aIsPinned)))
        continue;

      let storage, storageItemCount = 0;
      try {
        var principal = Services.scriptSecurityManager.getCodebasePrincipal(uri);

        // Using getSessionStorageForPrincipal instead of getSessionStorageForURI
        // just to be able to pass aCreate = false, that avoids creation of the
        // sessionStorage object for the page earlier than the page really
        // requires it. It was causing problems while accessing a storage when
        // a page later changed its domain.
        storage = aDocShell.getSessionStorageForPrincipal(principal, "", false);
        if (storage)
          storageItemCount = storage.length;
      }
      catch (ex) { /* sessionStorage might throw if it's turned off, see bug 458954 */ }
      if (storageItemCount == 0)
        continue;

      let data = storageData[domain] = {};
      for (let j = 0; j < storageItemCount; j++) {
        try {
          let key = storage.key(j);
          let item = storage.getItem(key);
          data[key] = item;
        }
        catch (ex) { /* this currently throws for secured items (cf. bug 442048) */ }
      }
      hasContent = true;
    }

    if (hasContent)
      aTabData.storage = storageData;
  },

  /**
   * restores all sessionStorage "super cookies"
   * @param aStorageData
   *        Storage data to be restored
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   */
  deserialize: function ssto_deserialize(aStorageData, aDocShell) {
    for (let url in aStorageData) {
      let uri = Services.io.newURI(url, null, null);
      let storage = aDocShell.getSessionStorageForURI(uri, "");
      for (let key in aStorageData[url]) {
        try {
          storage.setItem(key, aStorageData[url][key]);
        }
        catch (ex) { Cu.reportError(ex); } // throws e.g. for URIs that can't have sessionStorage
      }
    }
  }
};

Object.freeze(SessionStorage);
