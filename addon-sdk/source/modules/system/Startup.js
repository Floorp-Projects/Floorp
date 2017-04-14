/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["Startup"];

var { utils: Cu, interfaces: Ci, classes: Cc } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { defer } = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

const { XulApp } = Cu.import("resource://gre/modules/commonjs/sdk/system/xul-app.jsm", {});

const appStartupSrv = Cc["@mozilla.org/toolkit/app-startup;1"]
                       .getService(Ci.nsIAppStartup);

const NAME2TOPIC = {
  'Firefox': 'sessionstore-windows-restored',
  'Fennec': 'sessionstore-windows-restored',
  'SeaMonkey': 'sessionstore-windows-restored',
  'Thunderbird': 'mail-startup-done',
  'Instantbird': 'xul-window-visible'
};

var Startup = {
  initialized: !appStartupSrv.startingUp
};
var exports = Startup;

var gOnceInitializedDeferred = defer();
exports.onceInitialized = gOnceInitializedDeferred.promise;

// Set 'final-ui-startup' as default topic for unknown applications
var appStartup = 'final-ui-startup';

if (Startup.initialized) {
  gOnceInitializedDeferred.resolve()
}
else {
  // Gets the topic that fit best as application startup event, in according with
  // the current application (e.g. Firefox, Fennec, Thunderbird...)
  for (let name of Object.keys(NAME2TOPIC)) {
    if (XulApp.is(name)) {
      appStartup = NAME2TOPIC[name];
      break;
    }
  }

  let listener = function (subject, topic) {
    Services.obs.removeObserver(this, topic);
    Startup.initialized = true;
    Services.tm.currentThread.dispatch(() => gOnceInitializedDeferred.resolve(),
                                       Ci.nsIThread.DISPATCH_NORMAL);
  }

  Services.obs.addObserver(listener, appStartup);
}
