/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci, Cu } = require("chrome");
const { defer } = require("../core/promise");

const getZipReader = function getZipReader(aFile) {
  let { promise, resolve, reject } = defer();
  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                      createInstance(Ci.nsIZipReader);
  try {
    zipReader.open(aFile);
  }
  catch(e){
    reject(e);
  }
  resolve(zipReader);
  return promise;
};
exports.getZipReader = getZipReader;
