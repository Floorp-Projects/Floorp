/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, Cu, Cr } = require("chrome");
const self = require("../self");
const prefs = require("../preferences/service");
const { merge } = require("../util/object");
const { ConsoleAPI } = Cu.import("resource://gre/modules/devtools/Console.jsm");

const DEFAULT_LOG_LEVEL = "error";
const ADDON_LOG_LEVEL_PREF = "extensions." + self.id + ".sdk.console.logLevel";
const SDK_LOG_LEVEL_PREF = "extensions.sdk.console.logLevel";

let logLevel = DEFAULT_LOG_LEVEL;
function setLogLevel() {
  logLevel = prefs.get(ADDON_LOG_LEVEL_PREF,
                           prefs.get(SDK_LOG_LEVEL_PREF,
                                     DEFAULT_LOG_LEVEL));
}
setLogLevel();

let logLevelObserver = {
  QueryInterface: function(iid) {
    if (!iid.equals(Ci.nsIObserver) &&
        !iid.equals(Ci.nsISupportsWeakReference) &&
        !iid.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },
  observe: function(subject, topic, data) {
    setLogLevel();
  }
};
let branch = Cc["@mozilla.org/preferences-service;1"].
             getService(Ci.nsIPrefService).
             getBranch(null);
branch.addObserver(ADDON_LOG_LEVEL_PREF, logLevelObserver, true);
branch.addObserver(SDK_LOG_LEVEL_PREF, logLevelObserver, true);

function PlainTextConsole(print) {

  let consoleOptions = {
    prefix: self.name + ": ",
    maxLogLevel: logLevel,
    dump: print
  };
  let console = new ConsoleAPI(consoleOptions);

  // As we freeze the console object, we can't modify this property afterward
  Object.defineProperty(console, "maxLogLevel", {
    get: function() {
      return logLevel;
    }
  });

  // We defined the `__exposedProps__` in our console chrome object.
  // Although it seems redundant, because we use `createObjectIn` too, in
  // worker.js, we are following what `ConsoleAPI` does. See:
  // http://mxr.mozilla.org/mozilla-central/source/dom/base/ConsoleAPI.js#132
  //
  // Meanwhile we're investigating with the platform team if `__exposedProps__`
  // are needed, or are just a left-over.

  console.__exposedProps__ = Object.keys(ConsoleAPI.prototype).reduce(function(exposed, prop) {
    exposed[prop] = "r";
    return exposed;
  }, {});

  Object.freeze(console);
  return console;
};
exports.PlainTextConsole = PlainTextConsole;
