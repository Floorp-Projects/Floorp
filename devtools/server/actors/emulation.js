/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const protocol = require("devtools/shared/protocol");
const { emulationSpec } = require("devtools/shared/specs/emulation");
const { TouchSimulator } = require("devtools/server/actors/emulation/touch-simulator");

/**
 * This actor overrides various browser features to simulate different environments to
 * test how pages perform under various conditions.
 *
 * The design below, which saves the previous value of each property before setting, is
 * needed because it's possible to have multiple copies of this actor for a single page.
 * When some instance of this actor changes a property, we want it to be able to restore
 * that property to the way it was found before the change.
 *
 * A subtle aspect of the code below is that all get* methods must return non-undefined
 * values, so that the absence of a previous value can be distinguished from the value for
 * "no override" for each of the properties.
 */
const EmulationActor = protocol.ActorClassWithSpec(emulationSpec, {

  initialize(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.docShell = targetActor.docShell;
    this.touchSimulator = new TouchSimulator(targetActor.chromeEventHandler);
  },

  destroy() {
    this.clearDPPXOverride();
    this.clearNetworkThrottling();
    this.clearTouchEventsOverride();
    this.clearUserAgentOverride();
    this.targetActor = null;
    this.docShell = null;
    this.touchSimulator = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Retrieve the console actor for this tab.  This allows us to expose network throttling
   * as part of emulation settings, even though it's internally connected to the network
   * monitor, which for historical reasons is part of the console actor.
   */
  get _consoleActor() {
    if (this.targetActor.exited) {
      return null;
    }
    const form = this.targetActor.form();
    return this.conn._getOrCreateActor(form.consoleActor);
  },

  /* DPPX override */

  _previousDPPXOverride: undefined,

  setDPPXOverride(dppx) {
    if (this.getDPPXOverride() === dppx) {
      return false;
    }

    if (this._previousDPPXOverride === undefined) {
      this._previousDPPXOverride = this.getDPPXOverride();
    }

    this.docShell.contentViewer.overrideDPPX = dppx;

    return true;
  },

  getDPPXOverride() {
    return this.docShell.contentViewer.overrideDPPX;
  },

  clearDPPXOverride() {
    if (this._previousDPPXOverride !== undefined) {
      return this.setDPPXOverride(this._previousDPPXOverride);
    }

    return false;
  },

  /* Network Throttling */

  _previousNetworkThrottling: undefined,

  /**
   * Transform the RDP format into the internal format and then set network throttling.
   */
  setNetworkThrottling({ downloadThroughput, uploadThroughput, latency }) {
    const throttleData = {
      latencyMean: latency,
      latencyMax: latency,
      downloadBPSMean: downloadThroughput,
      downloadBPSMax: downloadThroughput,
      uploadBPSMean: uploadThroughput,
      uploadBPSMax: uploadThroughput,
    };
    return this._setNetworkThrottling(throttleData);
  },

  _setNetworkThrottling(throttleData) {
    const current = this._getNetworkThrottling();
    // Check if they are both objects or both null
    let match = throttleData == current;
    // If both objects, check all entries
    if (match && current && throttleData) {
      match = Object.entries(current).every(([ k, v ]) => {
        return throttleData[k] === v;
      });
    }
    if (match) {
      return false;
    }

    if (this._previousNetworkThrottling === undefined) {
      this._previousNetworkThrottling = current;
    }

    const consoleActor = this._consoleActor;
    if (!consoleActor) {
      return false;
    }
    consoleActor.startListeners({
      listeners: [ "NetworkActivity" ],
    });
    consoleActor.setPreferences({
      preferences: {
        "NetworkMonitor.throttleData": throttleData,
      }
    });
    return true;
  },

  /**
   * Get network throttling and then transform the internal format into the RDP format.
   */
  getNetworkThrottling() {
    const throttleData = this._getNetworkThrottling();
    if (!throttleData) {
      return null;
    }
    const { downloadBPSMax, uploadBPSMax, latencyMax } = throttleData;
    return {
      downloadThroughput: downloadBPSMax,
      uploadThroughput: uploadBPSMax,
      latency: latencyMax,
    };
  },

  _getNetworkThrottling() {
    const consoleActor = this._consoleActor;
    if (!consoleActor) {
      return null;
    }
    const prefs = consoleActor.getPreferences({
      preferences: [ "NetworkMonitor.throttleData" ],
    });
    return prefs.preferences["NetworkMonitor.throttleData"] || null;
  },

  clearNetworkThrottling() {
    if (this._previousNetworkThrottling !== undefined) {
      return this._setNetworkThrottling(this._previousNetworkThrottling);
    }

    return false;
  },

  /* Touch events override */

  _previousTouchEventsOverride: undefined,

  setTouchEventsOverride(flag) {
    if (this.getTouchEventsOverride() == flag) {
      return false;
    }
    if (this._previousTouchEventsOverride === undefined) {
      this._previousTouchEventsOverride = this.getTouchEventsOverride();
    }

    // Start or stop the touch simulator depending on the override flag
    if (flag == Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED) {
      this.touchSimulator.start();
    } else {
      this.touchSimulator.stop();
    }

    this.docShell.touchEventsOverride = flag;
    return true;
  },

  getTouchEventsOverride() {
    return this.docShell.touchEventsOverride;
  },

  clearTouchEventsOverride() {
    if (this._previousTouchEventsOverride !== undefined) {
      return this.setTouchEventsOverride(this._previousTouchEventsOverride);
    }
    return false;
  },

  /* User agent override */

  _previousUserAgentOverride: undefined,

  setUserAgentOverride(userAgent) {
    if (this.getUserAgentOverride() == userAgent) {
      return false;
    }
    if (this._previousUserAgentOverride === undefined) {
      this._previousUserAgentOverride = this.getUserAgentOverride();
    }
    this.docShell.customUserAgent = userAgent;
    return true;
  },

  getUserAgentOverride() {
    return this.docShell.customUserAgent;
  },

  clearUserAgentOverride() {
    if (this._previousUserAgentOverride !== undefined) {
      return this.setUserAgentOverride(this._previousUserAgentOverride);
    }
    return false;
  },

});

exports.EmulationActor = EmulationActor;
