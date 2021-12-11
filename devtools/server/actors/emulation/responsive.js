/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { responsiveSpec } = require("devtools/shared/specs/responsive");

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
const ResponsiveActor = protocol.ActorClassWithSpec(responsiveSpec, {
  initialize(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
    this.docShell = targetActor.docShell;
  },

  destroy() {
    this.clearNetworkThrottling();

    this.targetActor = null;
    this.docShell = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Retrieve the console actor for this tab.  This allows us to expose network throttling
   * as part of emulation settings, even though it's internally connected to the network
   * monitor, which for historical reasons is part of the console actor.
   */
  get _consoleActor() {
    if (this.targetActor.isDestroyed()) {
      return null;
    }
    const form = this.targetActor.form();
    return this.conn._getOrCreateActor(form.consoleActor);
  },

  get win() {
    return this.docShell.chromeEventHandler.ownerGlobal;
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
      match = Object.entries(current).every(([k, v]) => {
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
    consoleActor.startListeners(["NetworkActivity"]);
    consoleActor.setPreferences({
      "NetworkMonitor.throttleData": throttleData,
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
    const prefs = consoleActor.getPreferences(["NetworkMonitor.throttleData"]);
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

  /**
   * Set the current element picker state.
   *
   * True means the element picker is currently active and we should not be emulating
   * touch events.
   * False means the element picker is not active and it is ok to emulate touch events.
   *
   * This actor method is meant to be called by the DevTools front-end. The reason for
   * this is the following:
   * RDM is the only current consumer of the touch simulator. RDM instantiates this actor
   * on its own, whether or not the Toolbox is opened. That means it does so in its own
   * DevTools Server instance.
   * When the Toolbox is running, it uses a different DevToolsServer. Therefore, it is not
   * possible for the touch simulator to know whether the picker is active or not. This
   * state has to be sent by the client code of the Toolbox to this actor.
   * If a future use case arises where we want to use the touch simulator from the Toolbox
   * too, then we could add code in here to detect the picker mode as described in
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1409085#c3

   * @param {Boolean} state
   * @param {String} pickerType
   */
  setElementPickerState(state, pickerType) {
    this.targetActor.touchSimulator.setElementPickerState(state, pickerType);
  },

  /**
   * Dispatches an "orientationchange" event.
   */
  async dispatchOrientationChangeEvent() {
    const { CustomEvent } = this.win;
    const orientationChangeEvent = new CustomEvent("orientationchange");
    this.win.dispatchEvent(orientationChangeEvent);
  },
});

exports.ResponsiveActor = ResponsiveActor;
