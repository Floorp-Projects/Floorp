/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const protocol = require("devtools/shared/protocol");
const { emulationSpec } = require("devtools/shared/specs/emulation");
const { SimulatorCore } = require("devtools/shared/touch/simulator-core");

let EmulationActor = protocol.ActorClassWithSpec(emulationSpec, {
  initialize(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.docShell = tabActor.docShell;
    this.simulatorCore = new SimulatorCore(tabActor.chromeEventHandler);
  },

  /* Touch events override */

  _previousTouchEventsOverride: null,

  setTouchEventsOverride(flag) {
    if (this.docShell.touchEventsOverride == flag) {
      return false;
    }
    if (this._previousTouchEventsOverride === null) {
      this._previousTouchEventsOverride = this.docShell.touchEventsOverride;
    }

    // Start or stop the touch simulator depending on the override flag
    if (flag == Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED) {
      this.simulatorCore.start();
    } else {
      this.simulatorCore.stop();
    }

    this.docShell.touchEventsOverride = flag;
    return true;
  },

  getTouchEventsOverride() {
    return this.docShell.touchEventsOverride;
  },

  clearTouchEventsOverride() {
    if (this._previousTouchEventsOverride !== null) {
      return this.setTouchEventsOverride(this._previousTouchEventsOverride);
    }
    return false;
  },

  /* User agent override */

  _previousUserAgentOverride: null,

  setUserAgentOverride(userAgent) {
    if (this.docShell.customUserAgent == userAgent) {
      return false;
    }
    if (this._previousUserAgentOverride === null) {
      this._previousUserAgentOverride = this.docShell.customUserAgent;
    }
    this.docShell.customUserAgent = userAgent;
    return true;
  },

  getUserAgentOverride() {
    return this.docShell.customUserAgent;
  },

  clearUserAgentOverride() {
    if (this._previousUserAgentOverride !== null) {
      return this.setUserAgentOverride(this._previousUserAgentOverride);
    }
    return false;
  },

  disconnect() {
    this.destroy();
  },

  destroy() {
    this.clearTouchEventsOverride();
    this.clearUserAgentOverride();
    this.docShell = null;
    this.simulatorCore = null;
    protocol.Actor.prototype.destroy.call(this);
  },
});

exports.EmulationActor = EmulationActor;
