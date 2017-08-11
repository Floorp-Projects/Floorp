/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file only exists to support add-ons which import this module at a
 * specific path.
 */

(function (factory) {
  // Module boilerplate
  if (this.module && module.id.indexOf("event-emitter") >= 0) {
    // require
    factory.call(this, require, exports, module);
  } else {
    // Cu.import
    const Cu = Components.utils;
    const { require } =
      Cu.import("resource://devtools/shared/Loader.jsm", {});
    this.isWorker = false;
    this.promise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;
    factory.call(this, require, this, { exports: this });
    this.EXPORTED_SYMBOLS = ["EventEmitter"];
  }
}).call(this, function (require, exports, module) {
  const { Cu } = require("chrome");
  const Services = require("Services");
  const WARNING_PREF = "devtools.migration.warnings";
  // Cu and Services aren't accessible from workers
  if (Cu && Services && Services.prefs &&
      Services.prefs.getBoolPref(WARNING_PREF)) {
    const { Deprecated } =
      Cu.import("resource://gre/modules/Deprecated.jsm", {});
    Deprecated.warning("This path to event-emitter.js is deprecated.  Please " +
                       "use require(\"devtools/shared/old-event-emitter\") to " +
                       "load this module.",
                       "https://bugzil.la/912121");
  }

  const EventEmitter = require("devtools/shared/old-event-emitter");
  this.EventEmitter = EventEmitter;
  module.exports = EventEmitter;
});
