/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/devtools/event-emitter.js");

/**
 * TODO (Bug 1132453) The `Simulator` module is deprecated, and should be
 * removed once all simulator addons stop using it (see bug 1132452).
 *
 * If you want to register, unregister, or otherwise deal with installed
 * simulators, please use the `Simulators` module defined in:
 *
 *   browser/devtools/webide/modules/simulators.js
 */

const EXPORTED_SYMBOLS = ["Simulator"];

const Simulator = {
  _simulators: {},

  register: function (name, simulator) {
    // simulators register themselves as "Firefox OS X.Y"
    this._simulators[name] = simulator;
    this.emit("register", name);
  },

  unregister: function (name) {
    delete this._simulators[name];
    this.emit("unregister", name);
  },

  availableNames: function () {
    return Object.keys(this._simulators).sort();
  },

  getByName: function (name) {
    return this._simulators[name];
  },
};

EventEmitter.decorate(Simulator);
