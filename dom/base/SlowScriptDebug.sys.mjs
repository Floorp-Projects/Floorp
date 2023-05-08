/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function SlowScriptDebug() {}

SlowScriptDebug.prototype = {
  classDescription: "Slow script debug handler",
  QueryInterface: ChromeUtils.generateQI(["nsISlowScriptDebug"]),

  get activationHandler() {
    return this._activationHandler;
  },
  set activationHandler(cb) {
    this._activationHandler = cb;
  },

  get remoteActivationHandler() {
    return this._remoteActivationHandler;
  },
  set remoteActivationHandler(cb) {
    this._remoteActivationHandler = cb;
  },
};
