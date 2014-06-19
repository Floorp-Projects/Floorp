/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu, CC} = require("chrome");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const {Services} = Cu.import("resource://gre/modules/Services.jsm");

const XMLHttpRequest = CC("@mozilla.org/xmlextras/xmlhttprequest;1");

function getJSON(bypassCache, pref) {
  if (!bypassCache) {
    try {
      let str = Services.prefs.getCharPref(pref + "_cache");
      let json = JSON.parse(str);
      return promise.resolve(json);
    } catch(e) {/* no pref or invalid json. Let's continue */}
  }


  let deferred = promise.defer();

  let xhr = new XMLHttpRequest();

  xhr.onload = () => {
    let json;
    try {
      json = JSON.parse(xhr.responseText);
    } catch(e) {
      return deferred.reject("Not valid JSON");
    }
    Services.prefs.setCharPref(pref + "_cache", xhr.responseText);
    deferred.resolve(json);
  }

  xhr.onerror = (e) => {
    deferred.reject("Network error");
  }

  xhr.open("get", Services.prefs.getCharPref(pref));
  xhr.send();

  return deferred.promise;
}



exports.GetTemplatesJSON = function(bypassCache) {
  return getJSON(bypassCache, "devtools.webide.templatesURL");
}

exports.GetAddonsJSON = function(bypassCache) {
  return getJSON(bypassCache, "devtools.webide.addonsURL");
}
