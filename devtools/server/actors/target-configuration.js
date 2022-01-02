/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const {
  targetConfigurationSpec,
} = require("devtools/shared/specs/target-configuration");
const {
  SessionDataHelpers,
} = require("devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { SUPPORTED_DATA } = SessionDataHelpers;
const { TARGET_CONFIGURATION } = SUPPORTED_DATA;
const Services = require("Services");

// List of options supported by this target configuration actor.
/* eslint sort-keys: "error" */
const SUPPORTED_OPTIONS = {
  // Disable network request caching.
  cacheDisabled: true,
  // Enable color scheme simulation.
  colorSchemeSimulation: true,
  // Set a custom user agent
  customUserAgent: true,
  // Is the client using the new performance panel.
  isNewPerfPanelEnabled: true,
  // Enable JavaScript
  javascriptEnabled: true,
  // Force a custom device pixel ratio (used in RDM). Set to null to restore origin ratio.
  overrideDPPX: true,
  // Enable print simulation mode.
  printSimulationEnabled: true,
  // Override navigator.maxTouchPoints (used in RDM and doesn't apply if RDM isn't enabled)
  rdmPaneMaxTouchPoints: true,
  // Page orientation (used in RDM and doesn't apply if RDM isn't enabled)
  rdmPaneOrientation: true,
  // Enable allocation tracking, if set, contains an object defining the tracking configurations
  recordAllocations: true,
  // Reload the page when the touch simulation state changes (only works alongside touchEventsOverride)
  reloadOnTouchSimulationToggle: true,
  // Restore focus in the page after closing DevTools.
  restoreFocus: true,
  // Enable service worker testing over HTTP (instead of HTTPS only).
  serviceWorkersTestingEnabled: true,
  // Enable touch events simulation
  touchEventsOverride: true,
};
/* eslint-disable sort-keys */

/**
 * This actor manages the configuration flags which apply to DevTools targets.
 *
 * Configuration flags should be applied to all concerned targets when the
 * configuration is updated, and new targets should also be able to read the
 * flags when they are created. The flags will be forwarded to the WatcherActor
 * and stored as TARGET_CONFIGURATION data entries.
 * Some flags will be set directly set from this actor, in the parent process
 * (see _updateParentProcessConfiguration), and others will be set from the target actor,
 * in the content process.
 *
 * @constructor
 *
 */
const TargetConfigurationActor = ActorClassWithSpec(targetConfigurationSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);

    this._onBrowsingContextAttached = this._onBrowsingContextAttached.bind(
      this
    );
    // We need to be notified of new browsing context being created so we can re-set flags
    // we already set on the "previous" browsing context. We're using this event  as it's
    // emitted very early in the document lifecycle (i.e. before any script on the page is
    // executed), which is not the case for "window-global-created" for example.
    Services.obs.addObserver(
      this._onBrowsingContextAttached,
      "browsing-context-attached"
    );

    this._browsingContext = this.watcherActor.browserElement?.browsingContext;
  },

  form() {
    return {
      actor: this.actorID,
      configuration: this._getConfiguration(),
      traits: { supportedOptions: SUPPORTED_OPTIONS },
    };
  },

  /**
   * Returns whether or not this actor should handle the flag that should be set on the
   * BrowsingContext in the parent process.
   *
   * @returns {Boolean}
   */
  _shouldHandleConfigurationInParentProcess() {
    // Only handle parent process configuration if the watcherActor is tied to a
    // browser element (i.e. we're *not* in the Browser Toolbox)
    return this.watcherActor.context.type == "browser-element";
  },

  /**
   * Event handler for attached browsing context. This will be called when
   * a new browsing context is created that we might want to handle
   * (e.g. when navigating to a page with Cross-Origin-Opener-Policy header)
   */
  _onBrowsingContextAttached(browsingContext) {
    if (!this._shouldHandleConfigurationInParentProcess()) {
      return;
    }

    // We only want to set flags on top-level browsing context. The platform
    // will take care of propagating it to the entire browsing contexts tree.
    if (browsingContext.parent) {
      return;
    }

    // If the watcher is bound to one browser element (i.e. a tab), ignore
    // updates related to other browser elements
    if (
      this.watcherActor.context.type == "browser-element" &&
      browsingContext.browserId != this.watcherActor.context.browserId
    ) {
      return;
    }

    const rdmEnabledInPreviousBrowsingContext = this._browsingContext.inRDMPane;

    // We need to store the browsing context as this.watcherActor.browserElement.browsingContext
    // can still refer to the previous browsing context at this point.
    this._browsingContext = browsingContext;

    // If `inRDMPane` was set in the previous browsing context, set it again on the new one,
    // otherwise some RDM-related configuration won't be applied (e.g. orientation).
    if (rdmEnabledInPreviousBrowsingContext) {
      this._browsingContext.inRDMPane = true;
    }
    this._updateParentProcessConfiguration(this._getConfiguration());
  },

  _getConfiguration() {
    const targetConfigurationData = this.watcherActor.getSessionDataForType(
      TARGET_CONFIGURATION
    );
    if (!targetConfigurationData) {
      return {};
    }

    const cfgMap = {};
    for (const { key, value } of targetConfigurationData) {
      cfgMap[key] = value;
    }
    return cfgMap;
  },

  /**
   *
   * @param {Object} configuration
   * @returns Promise<Object> Applied configuration object
   */
  async updateConfiguration(configuration) {
    const cfgArray = Object.keys(configuration)
      .filter(key => {
        if (!SUPPORTED_OPTIONS[key]) {
          console.warn(`Unsupported option for TargetConfiguration: ${key}`);
          return false;
        }
        return true;
      })
      .map(key => ({ key, value: configuration[key] }));

    this._updateParentProcessConfiguration(configuration);
    await this.watcherActor.addDataEntry(TARGET_CONFIGURATION, cfgArray);
    return this._getConfiguration();
  },

  /**
   *
   * @param {Object} configuration: See `updateConfiguration`
   */
  _updateParentProcessConfiguration(configuration) {
    if (!this._shouldHandleConfigurationInParentProcess()) {
      return;
    }

    let shouldReload = false;
    for (const [key, value] of Object.entries(configuration)) {
      switch (key) {
        case "colorSchemeSimulation":
          this._setColorSchemeSimulation(value);
          break;
        case "customUserAgent":
          this._setCustomUserAgent(value);
          break;
        case "javascriptEnabled":
          if (value !== undefined) {
            // This flag requires a reload in order to take full effect,
            // so reload if it has changed.
            if (value != this.isJavascriptEnabled()) {
              shouldReload = true;
            }
            this._setJavascriptEnabled(value);
          }
          break;
        case "overrideDPPX":
          this._setDPPXOverride(value);
          break;
        case "printSimulationEnabled":
          this._setPrintSimulationEnabled(value);
          break;
        case "rdmPaneMaxTouchPoints":
          this._setRDMPaneMaxTouchPoints(value);
          break;
        case "rdmPaneOrientation":
          this._setRDMPaneOrientation(value);
          break;
        case "serviceWorkersTestingEnabled":
          this._setServiceWorkersTestingEnabled(value);
          break;
        case "touchEventsOverride":
          this._setTouchEventsOverride(value);
          break;
        case "cacheDisabled":
          this._setCacheDisabled(value);
          break;
      }
    }

    if (shouldReload) {
      this._browsingContext.reload(Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
    }
  },

  _restoreParentProcessConfiguration() {
    if (!this._shouldHandleConfigurationInParentProcess()) {
      return;
    }

    this._setServiceWorkersTestingEnabled(false);
    this._setPrintSimulationEnabled(false);
    this._setCacheDisabled(false);

    // Restore the color scheme simulation only if it was explicitly updated
    // by this actor. This will avoid side effects caused when destroying additional
    // targets (e.g. RDM target, WebExtension target, …).
    // TODO: We may want to review other configuration values to see if we should use
    // the same pattern (Bug 1701553).
    if (this._resetColorSchemeSimulationOnDestroy) {
      this._setColorSchemeSimulation(null);
    }

    // Restore the user agent only if it was explicitly updated by this specific actor.
    if (this._initialUserAgent !== undefined) {
      this._setCustomUserAgent(this._initialUserAgent);
    }

    // Restore the origin device pixel ratio only if it was explicitly updated by this
    // specific actor.
    if (this._initialDPPXOverride !== undefined) {
      this._setDPPXOverride(this._initialDPPXOverride);
    }

    if (this._initialJavascriptEnabled !== undefined) {
      this._setJavascriptEnabled(this._initialJavascriptEnabled);
    }

    if (this._initialTouchEventsOverride !== undefined) {
      this._setTouchEventsOverride(this._initialTouchEventsOverride);
    }
  },

  /**
   * Disable or enable the service workers testing features.
   */
  _setServiceWorkersTestingEnabled(enabled) {
    if (this._browsingContext.serviceWorkersTestingEnabled != enabled) {
      this._browsingContext.serviceWorkersTestingEnabled = enabled;
    }
  },

  /**
   * Disable or enable the print simulation.
   */
  _setPrintSimulationEnabled(enabled) {
    const value = enabled ? "print" : "";
    if (this._browsingContext.mediumOverride != value) {
      this._browsingContext.mediumOverride = value;
    }
  },

  /**
   * Disable or enable the color-scheme simulation.
   */
  _setColorSchemeSimulation(override) {
    const value = override || "none";
    if (this._browsingContext.prefersColorSchemeOverride != value) {
      this._browsingContext.prefersColorSchemeOverride = value;
      this._resetColorSchemeSimulationOnDestroy = true;
    }
  },

  /**
   * Set a custom user agent on the page
   *
   * @param {String} userAgent: The user agent to set on the page. If null, will reset the
   *                 user agent to its original value.
   * @returns {Boolean} Whether the user agent was changed or not.
   */
  _setCustomUserAgent(userAgent = "") {
    if (this._browsingContext.customUserAgent === userAgent) {
      return;
    }

    if (this._initialUserAgent === undefined) {
      this._initialUserAgent = this._browsingContext.customUserAgent;
    }

    this._browsingContext.customUserAgent = userAgent;
  },

  isJavascriptEnabled() {
    return this._browsingContext.allowJavascript;
  },

  _setJavascriptEnabled(allow) {
    if (this._initialJavascriptEnabled === undefined) {
      this._initialJavascriptEnabled = this._browsingContext.allowJavascript;
    }
    if (allow !== undefined) {
      this._browsingContext.allowJavascript = allow;
    }
  },

  /* DPPX override */
  _setDPPXOverride(dppx) {
    if (this._browsingContext.overrideDPPX === dppx) {
      return;
    }

    if (!dppx && this._initialDPPXOverride) {
      dppx = this._initialDPPXOverride;
    } else if (dppx !== undefined && this._initialDPPXOverride === undefined) {
      this._initialDPPXOverride = this._browsingContext.overrideDPPX;
    }

    if (dppx !== undefined) {
      this._browsingContext.overrideDPPX = dppx;
    }
  },

  /**
   * Set the touchEventsOverride on the browsing context.
   *
   * @param {String} flag: See BrowsingContext.webidl `TouchEventsOverride` enum for values.
   */
  _setTouchEventsOverride(flag) {
    if (this._browsingContext.touchEventsOverride === flag) {
      return;
    }

    if (!flag && this._initialTouchEventsOverride) {
      flag = this._initialTouchEventsOverride;
    } else if (
      flag !== undefined &&
      this._initialTouchEventsOverride === undefined
    ) {
      this._initialTouchEventsOverride = this._browsingContext.touchEventsOverride;
    }

    if (flag !== undefined) {
      this._browsingContext.touchEventsOverride = flag;
    }
  },

  /**
   * Overrides navigator.maxTouchPoints.
   * Note that we don't need to reset the original value when the actor is destroyed,
   * as it's directly handled by the platform when RDM is closed.
   *
   * @param {Integer} maxTouchPoints
   */
  _setRDMPaneMaxTouchPoints(maxTouchPoints) {
    this._browsingContext.setRDMPaneMaxTouchPoints(maxTouchPoints);
  },

  /**
   * Set an orientation and an angle on the browsing context. This will be applied only
   * if Responsive Design Mode is enabled.
   *
   * @param {Object} options
   * @param {String} options.type: The orientation type of the rotated device.
   * @param {Number} options.angle: The rotated angle of the device.
   */
  _setRDMPaneOrientation({ type, angle }) {
    this._browsingContext.setRDMPaneOrientation(type, angle);
  },

  /**
   * Disable or enable the cache via the browsing context.
   *
   * @param {Boolean} disabled: The state the cache should be changed to
   */
  _setCacheDisabled(disabled) {
    const value = disabled
      ? Ci.nsIRequest.LOAD_BYPASS_CACHE
      : Ci.nsIRequest.LOAD_NORMAL;
    if (this._browsingContext.defaultLoadFlags != value) {
      this._browsingContext.defaultLoadFlags = value;
    }
  },

  destroy() {
    Services.obs.removeObserver(
      this._onBrowsingContextAttached,
      "browsing-context-attached"
    );
    this._restoreParentProcessConfiguration();
    Actor.prototype.destroy.call(this);
  },
});

exports.TargetConfigurationActor = TargetConfigurationActor;
