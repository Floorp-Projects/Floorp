/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const promise = require("promise");

/**
 * Map of pending requests. Used mainly by tests to wait
 * till things are ready.
 */
var promises = new Map();

/**
 * This object is used to fetch network data from the backend.
 * Communication with the chrome scope is based on message
 * exchange.
 */
var DataProvider = {
  hasPendingRequests: function () {
    return promises.size > 0;
  },

  requestData: function (client, actor, method) {
    let key = actor + ":" + method;
    let p = promises.get(key);
    if (p) {
      return p;
    }

    let deferred = promise.defer();
    let realMethodName = "get" + method.charAt(0).toUpperCase() +
      method.slice(1);

    if (!client[realMethodName]) {
      return null;
    }

    client[realMethodName](actor, response => {
      promises.delete(key);
      deferred.resolve(response);
    });

    promises.set(key, deferred.promise);
    return deferred.promise;
  },

  resolveString: function (client, stringGrip) {
    let key = stringGrip.actor + ":getString";
    let p = promises.get(key);
    if (p) {
      return p;
    }

    p = client.getString(stringGrip).then(result => {
      promises.delete(key);
      return result;
    });

    promises.set(key, p);
    return p;
  },
};

// Exports from this module
module.exports = DataProvider;
