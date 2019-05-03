/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const protocol = require("devtools/shared/protocol");
const { emulationSpec } = require("devtools/shared/specs/emulation");

loader.lazyRequireGetter(this, "TouchSimulator", "devtools/server/actors/emulation/touch-simulator", true);

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

    this.onWillNavigate = this.onWillNavigate.bind(this);
    this.onWindowReady = this.onWindowReady.bind(this);

    this.targetActor.on("will-navigate", this.onWillNavigate);
    this.targetActor.on("window-ready", this.onWindowReady);
  },

  destroy() {
    this.stopPrintMediaSimulation();
    this.clearDPPXOverride();
    this.clearNetworkThrottling();
    this.clearTouchEventsOverride();
    this.clearMetaViewportOverride();
    this.clearUserAgentOverride();

    this.targetActor.off("will-navigate", this.onWillNavigate);
    this.targetActor.off("window-ready", this.onWindowReady);

    this.targetActor = null;
    this.docShell = null;
    this._touchSimulator = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Retrieve the console actor for this tab.  This allows us to expose network throttling
   * as part of emulation settings, even though it's internally connected to the network
   * monitor, which for historical reasons is part of the console actor.
   */
  get _consoleActor() {
    if (this.targetActor.exited || !this.targetActor.actorID) {
      return null;
    }
    const form = this.targetActor.form();
    return this.conn._getOrCreateActor(form.consoleActor);
  },

  get touchSimulator() {
    if (!this._touchSimulator) {
      this._touchSimulator = new TouchSimulator(this.targetActor.chromeEventHandler);
    }

    return this._touchSimulator;
  },

  onWillNavigate({ isTopLevel }) {
    // Make sure that print simulation is stopped before navigating to another page. We
    // need to do this since the browser will cache the last state of the page in its
    // session history.
    if (this._printSimulationEnabled && isTopLevel) {
      this.stopPrintMediaSimulation(true);
    }
  },

  onWindowReady({ isTopLevel }) {
    // Since `emulateMedium` only works for the current page, we need to ensure persistent
    // print simulation for when the user navigates to a new page while its enabled.
    // To do this, we need to tell the page to begin print simulation before the DOM
    // content is available to the user:
    if (this._printSimulationEnabled && isTopLevel) {
      this.startPrintMediaSimulation();
    }
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
      },
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
   * Debugger Server instance.
   * When the Toolbox is running, it uses a different DebuggerServer. Therefore, it is not
   * possible for the touch simulator to know whether the picker is active or not. This
   * state has to be sent by the client code of the Toolbox to this actor.
   * If a future use case arises where we want to use the touch simulator from the Toolbox
   * too, then we could add code in here to detect the picker mode as described in
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1409085#c3
   * @param {Boolean} state
   */
  setElementPickerState(state) {
    this.touchSimulator.setElementPickerState(state);
  },

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

  /* Meta viewport override */

  _previousMetaViewportOverride: undefined,

  setMetaViewportOverride(flag) {
    if (this.getMetaViewportOverride() == flag) {
      return false;
    }
    if (this._previousMetaViewportOverride === undefined) {
      this._previousMetaViewportOverride = this.getMetaViewportOverride();
    }

    this.docShell.metaViewportOverride = flag;
    return true;
  },

  getMetaViewportOverride() {
    return this.docShell.metaViewportOverride;
  },

  clearMetaViewportOverride() {
    if (this._previousMetaViewportOverride !== undefined) {
      return this.setMetaViewportOverride(this._previousMetaViewportOverride);
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

  /* Simulating print media for the page */

  _printSimulationEnabled: false,

  getIsPrintSimulationEnabled() {
    return this._printSimulationEnabled;
  },

  async startPrintMediaSimulation() {
    this._printSimulationEnabled = true;
    this.targetActor.docShell.contentViewer.emulateMedium("print");
  },

  /**
   * Stop simulating print media for the current page.
   *
   * @param {Boolean} state
   *        Whether or not to set _printSimulationEnabled to false. If true, we want to
   *        stop simulation print media for the current page but NOT set
   *        _printSimulationEnabled to false. We do this specifically for the
   *        "will-navigate" event where we still want to continue simulating print when
   *        navigating to the next page. Defaults to false, meaning we want to completely
   *        stop print simulation.
   */
  async stopPrintMediaSimulation(state = false) {
    this._printSimulationEnabled = state;
    this.targetActor.docShell.contentViewer.stopEmulatingMedium();
  },

  /**
   * Simulates the "orientationchange" event when device screen is rotated.
   *
   * TODO: Update `window.screen.orientation` and `window.screen.angle` here.
   * See Bug 1357774.
   */
  simulateScreenOrientationChange() {
    const win = this.docShell.chromeEventHandler.ownerGlobal;
    const { CustomEvent } = win;
    const orientationChangeEvent = new CustomEvent("orientationchange");
    win.dispatchEvent(orientationChangeEvent);
  },
});

exports.EmulationActor = EmulationActor;
