/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function SlowScriptDebug() { }

SlowScriptDebug.prototype = {
  classID: Components.ID("{e740ddb4-18b4-4aac-8ae1-9b0f4320769d}"),
  classDescription: "Slow script debug handler",
  contractID: "@mozilla.org/dom/slow-script-debug;1",
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISlowScriptDebug]),

  get activationHandler()   { return this._activationHandler; },
  set activationHandler(cb) { return this._activationHandler = cb; },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SlowScriptDebug]);
