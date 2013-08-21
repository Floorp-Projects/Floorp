/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const { CC } = require('chrome');
const { id, name, prefixURI, rootURI, metadata,
        version, loadReason } = require('@loader/options');

const { readURISync } = require('./net/url');

const addonDataURI = prefixURI + name + '/data/';

function uri(path) {
  return addonDataURI + (path || '');
}


// Some XPCOM APIs require valid URIs as an argument for certain operations
// (see `nsILoginManager` for example). This property represents add-on
// associated unique URI string that can be used for that.
exports.uri = 'addon:' + id;
exports.id = id;
exports.name = name;
exports.loadReason = loadReason;
exports.version = version;
// If `rootURI` is jar:file://...!/ than add-on is packed.
exports.packed = rootURI.indexOf('jar:') === 0
exports.data = Object.freeze({
  url: uri,
  load: function read(path) {
    return readURISync(uri(path));
  }
});
exports.isPrivateBrowsingSupported = ((metadata.permissions || {})['private-browsing'] === true) ?
                                     true : false;
