/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function SlowScriptDebug() {}

SlowScriptDebug.prototype = {
  classDescription: "Slow script debug handler",
  QueryInterface: ChromeUtils.generateQI([Ci.nsISlowScriptDebug]),

  get activationHandler() {
    return this._activationHandler;
  },
  set activationHandler(cb) {
    return (this._activationHandler = cb);
  },

  get remoteActivationHandler() {
    return this._remoteActivationHandler;
  },
  set remoteActivationHandler(cb) {
    return (this._remoteActivationHandler = cb);
  },
};

var EXPORTED_SYMBOLS = ["SlowScriptDebug"];
