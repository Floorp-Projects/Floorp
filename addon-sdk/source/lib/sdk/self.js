/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const { CC } = require('chrome');
const options = require('@loader/options');

const { get } = require("./preferences/service");
const { readURISync } = require('./net/url');

const id = options.id;

const readPref = key => get("extensions." + id + ".sdk." + key);

const name = readPref("name") || options.name;
const version = readPref("version") || options.version;
const loadReason = readPref("load.reason") || options.loadReason;
const rootURI = readPref("rootURI") || options.rootURI || "";
const baseURI = readPref("baseURI") || options.prefixURI + name + "/"
const addonDataURI = baseURI + "data/";
const metadata = options.metadata || {};
const permissions = metadata.permissions || {};
const isPacked = rootURI && rootURI.indexOf("jar:") === 0;

const isPrivateBrowsingSupported = 'private-browsing' in permissions &&
                                   permissions['private-browsing'] === true;

const uri = (path="") =>
  path.includes(":") ? path : addonDataURI + path.replace(/^\.\//, "");

var preferencesBranch = ("preferences-branch" in metadata)
                            ? metadata["preferences-branch"]
                            : options.preferencesBranch

if (/[^\w{@}.-]/.test(preferencesBranch)) {
  preferencesBranch = id;
  console.warn("Ignoring preferences-branch (not a valid branch name)");
}

// Some XPCOM APIs require valid URIs as an argument for certain operations
// (see `nsILoginManager` for example). This property represents add-on
// associated unique URI string that can be used for that.
exports.uri = 'addon:' + id;
exports.id = id;
exports.preferencesBranch = preferencesBranch || id;
exports.name = name;
exports.loadReason = loadReason;
exports.version = version;
exports.packed = isPacked;
exports.data = Object.freeze({
  url: uri,
  load: function read(path) {
    return readURISync(uri(path));
  }
});
exports.isPrivateBrowsingSupported = isPrivateBrowsingSupported;
