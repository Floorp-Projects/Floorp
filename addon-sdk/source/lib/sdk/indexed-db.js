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

// Use XPCOM because `require("./url").URL` doesn't expose the raw uri object.
let principaluri = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService).
              newURI(PSEUDOURI, null, null);

let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
            .getService(Ci.nsIScriptSecurityManager);
let principal = ssm.createCodebasePrincipal(principaluri, {});

function toArray(args) {
  return Array.prototype.slice.call(args);
}

function openInternal(args, forPrincipal, deleting) {
  if (forPrincipal) {
    args = toArray(args);
  } else {
    args = [principal].concat(toArray(args));
  }
  if (args.length == 2) {
    args.push({ storage: "persistent" });
  } else if (!deleting && args.length >= 3 && typeof args[2] === "number") {
    args[2] = { version: args[2], storage: "persistent" };
  }

  if (deleting) {
    return indexedDB.deleteForPrincipal.apply(indexedDB, args);
  }

  return indexedDB.openForPrincipal.apply(indexedDB, args);
}

exports.indexedDB = Object.freeze({
  open: function () {
    return openInternal(arguments, false, false);
  },
  deleteDatabase: function () {
    return openInternal(arguments, false, true);
  },
  openForPrincipal: function () {
    return openInternal(arguments, true, false);
  },
  deleteForPrincipal: function () {
    return openInternal(arguments, true, true);
  },
  cmp: indexedDB.cmp.bind(indexedDB)
});

exports.IDBKeyRange = IDBKeyRange;
exports.DOMException = Ci.nsIDOMDOMException;
