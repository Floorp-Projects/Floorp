/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci } = require("chrome");
const { id } = require("./self");

// placeholder, copied from bootstrap.js
let sanitizeId = function(id){
  let uuidRe =
    /^\{([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\}$/;

  let domain = id.
    toLowerCase().
    replace(/@/g, "-at-").
    replace(/\./g, "-dot-").
    replace(uuidRe, "$1");

  return domain
};

const PSEUDOURI = "indexeddb://" + sanitizeId(id) // https://bugzilla.mozilla.org/show_bug.cgi?id=779197

// Firefox 26 and earlier releases don't support `indexedDB` in sandboxes
// automatically, so we need to inject `indexedDB` to `this` scope ourselves.
if (typeof(indexedDB) === "undefined") {
  Cc["@mozilla.org/dom/indexeddb/manager;1"].
    getService(Ci.nsIIndexedDatabaseManager).
    initWindowless(this);

  // Firefox 14 gets this with a prefix
  if (typeof(indexedDB) === "undefined")
    this.indexedDB = mozIndexedDB;
}

// Use XPCOM because `require("./url").URL` doesn't expose the raw uri object.
let principaluri = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService).
              newURI(PSEUDOURI, null, null);

let principal = Cc["@mozilla.org/scriptsecuritymanager;1"].
	               getService(Ci.nsIScriptSecurityManager).
	               getCodebasePrincipal(principaluri);

exports.indexedDB = Object.freeze({
  open: indexedDB.openForPrincipal.bind(indexedDB, principal),
  deleteDatabase: indexedDB.deleteForPrincipal.bind(indexedDB, principal),
  cmp: indexedDB.cmp
});

exports.IDBKeyRange = IDBKeyRange;
exports.DOMException = Ci.nsIDOMDOMException;
