/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci, Cr } = require("chrome");
const IOService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
const { isValidURI } = require("../url");
const { method } = require("../../method/core");

function newURI (uri) {
  if (!isValidURI(uri))
    throw new Error("malformed URI: " + uri);
  return IOService.newURI(uri, null, null);
}
exports.newURI = newURI;

let getURL = method('sdk/url:getURL');
getURL.define(String, function (url) url);
getURL.define(function (object) {
  return null;
});
exports.getURL = getURL;
