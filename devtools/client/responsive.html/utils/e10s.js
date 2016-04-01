/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");

module.exports = {

  waitForMessage(mm, message) {
    let deferred = promise.defer();

    let onMessage = event => {
      mm.removeMessageListener(message, onMessage);
      deferred.resolve();
    };
    mm.addMessageListener(message, onMessage);

    return deferred.promise;
  },

};
