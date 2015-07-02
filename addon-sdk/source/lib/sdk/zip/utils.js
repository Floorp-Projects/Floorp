/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");

function getZipReader(aFile) {
  return new Promise(resolve => {
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                        createInstance(Ci.nsIZipReader);
    zipReader.open(aFile);
    resolve(zipReader);
  });
};
exports.getZipReader = getZipReader;
