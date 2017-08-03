/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This indexedDB helper is a simplified version of sdk/indexed-db. It creates a DB with
 * a principal dedicated to DevTools.
 */

const { Cc, Ci } = require("chrome");

const PSEUDOURI = "indexeddb://fx-devtools";
const principaluri = Cc["@mozilla.org/network/io-service;1"]
                     .getService(Ci.nsIIOService).newURI(PSEUDOURI);

const ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
            .getService(Ci.nsIScriptSecurityManager);

const principal = ssm.createCodebasePrincipal(principaluri, {});

/**
 * Create the DevTools dedicated DB, by relying on the real indexedDB object passed as a
 * parameter here.
 *
 * @param {IDBFactory} indexedDB
 *        Real indexedDB object.
 * @return {Object} Wrapper object that implements IDBFactory methods, but for a devtools
 *         specific principal.
 */
exports.createDevToolsIndexedDB = function (indexedDB) {
  return Object.freeze({
    /**
     * Only the standard version of indexedDB.open is supported.
     */
    open(name, version) {
      let options = { storage: "persistent" };
      if (typeof version === "number") {
        options.version = version;
      }
      return indexedDB.openForPrincipal(principal, name, options);
    },
    /**
     * Only the standard version of indexedDB.deleteDatabase is supported.
     */
    deleteDatabase(name) {
      return indexedDB.deleteForPrincipal(principal, name, { storage: "persistent" });
    },
    cmp: indexedDB.cmp.bind(indexedDB),
  });
};
