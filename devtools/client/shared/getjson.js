/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {CC} = require("chrome");
const defer = require("devtools/shared/defer");
const promise = require("promise");
const Services = require("Services");

loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const XMLHttpRequest = CC("@mozilla.org/xmlextras/xmlhttprequest;1");

/**
 * Downloads and caches a JSON file from an URL given by a pref.
 *
 * @param {String} prefName
 *        The preference for the target URL
 *
 * @return {Promise}
 *         - Resolved with the JSON object in case of successful request
 *           or cache hit
 *         - Rejected with an error message in case of failure
 */
exports.getJSON = function (prefName) {
  let deferred = defer();
  let xhr = new XMLHttpRequest();

  // We used to store cached data in preferences, but now we use asyncStorage
  // Migration step: if it still exists, move this now useless preference in its
  // new location and clear it
  if (Services.prefs.prefHasUserValue(prefName + "_cache")) {
    let json = Services.prefs.getCharPref(prefName + "_cache");
    asyncStorage.setItem(prefName + "_cache", json).catch(function (e) {
      // Could not move the cache, let's log the error but continue
      console.error(e);
    });
    Services.prefs.clearUserPref(prefName + "_cache");
  }

  function readFromStorage(networkError) {
    asyncStorage.getItem(prefName + "_cache").then(function (json) {
      if (!json) {
        return promise.reject("Empty cache for " + prefName);
      }
      return deferred.resolve(json);
    }).catch(function (e) {
      deferred.reject("JSON not available, CDN error: " + networkError +
                      ", storage error: " + e);
    });
  }

  xhr.onload = () => {
    try {
      let json = JSON.parse(xhr.responseText);
      asyncStorage.setItem(prefName + "_cache", json).catch(function (e) {
        // Could not update cache, let's log the error but continue
        console.error(e);
      });
      deferred.resolve(json);
    } catch (e) {
      readFromStorage(e);
    }
  };

  xhr.onerror = (e) => {
    readFromStorage(e);
  };

  xhr.open("get", Services.prefs.getCharPref(prefName));
  xhr.send();

  return deferred.promise;
};
