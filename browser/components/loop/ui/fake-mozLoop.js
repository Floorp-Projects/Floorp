/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Faking the mozLoop object which doesn't exist in regular web pages.
 * @type {Object}
 */
navigator.mozLoop = {
  ensureRegistered: function() {},
  getLoopCharPref: function() {},
  getLoopBoolPref: function() {},
  releaseCallData: function() {},
  contacts: {
    getAll: function(callback) {
      callback(null, []);
    },
    on: function() {}
  }
};
