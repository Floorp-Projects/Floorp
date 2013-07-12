/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable",
  "engines": {
    "Firefox": "*"
  }
};

const { Cc, Ci, Cu } = require("chrome");
const { defer, reject } = require("../core/promise");
const FaviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                          getService(Ci.nsIFaviconService);
const AsyncFavicons = FaviconService.QueryInterface(Ci.mozIAsyncFavicons);
const { isValidURI } = require("../url");
const { newURI, getURL } = require("../url/utils");

/**
 * Takes an object of several possible types and
 * returns a promise that resolves to the page's favicon URI.
 * @param {String|Tab} object
 * @param {Function} (callback)
 * @returns {Promise}
 */

function getFavicon (object, callback) {
  let url = getURL(object);
  let deferred = defer();

  if (url && isValidURI(url)) {
    AsyncFavicons.getFaviconURLForPage(newURI(url), function (aURI) {
      if (aURI && aURI.spec)
        deferred.resolve(aURI.spec.toString());
      else
        deferred.reject(null);
    });
  } else {
    deferred.reject(null);
  }

  if (callback) deferred.promise.then(callback, callback);
  return deferred.promise;
}
exports.getFavicon = getFavicon;
