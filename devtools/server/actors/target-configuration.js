/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const {
  targetConfigurationSpec,
} = require("devtools/shared/specs/target-configuration");
const {
  WatchedDataHelpers,
} = require("devtools/server/actors/watcher/WatchedDataHelpers.jsm");
const { SUPPORTED_DATA } = WatchedDataHelpers;
const { TARGET_CONFIGURATION } = SUPPORTED_DATA;
const Services = require("Services");

// List of options supported by this target configuration actor.
const SUPPORTED_OPTIONS = {
  // Disable network request caching.
  cacheDisabled: true,
  // Enable color scheme simulation.
  colorSchemeSimulation: true,
  // Enable JavaScript
  javascriptEnabled: true,
  // Enable paint flashing mode.
  paintFlashing: true,
  // Enable print simulation mode.
  printSimulationEnabled: true,
  // Restore focus in the page after closing DevTools.
  restoreFocus: true,
  // Enable service worker testing over HTTP (instead of HTTPS only).
  serviceWorkersTestingEnabled: true,
};

/**
 * This actor manages the configuration flags which apply to DevTools targets.
 *
 * Configuration flags should be applied to all concerned targets when the
 * configuration is updated, and new targets should also be able to read the
 * flags when they are created. The flags will be forwarded to the WatcherActor
 * and stored as TARGET_CONFIGURATION data entries.
 * Some flags will be set directly set from this actor, in the parent process
 * (see _updateParentProcessConfiguration), nad others will be set from the target actor,
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
   * Event handler for attached browsing context. This will be called when
   * a new browsing context is created that we might want to handle
   * (e.g. when navigating to a page with Cross-Origin-Opener-Policy header)
   */
  _onBrowsingContextAttached(browsingContext) {
    // We only want to set flags on top-level browsing context. The platform
    // will take care of propagating it to the entire browsing contexts tree.
    if (browsingContext.parent) {
      return;
    }

    // If the watcher is bound to one browser element (i.e. a tab), ignore
    // updates related to other browser elements
    if (
      this.watcherActor.browserId &&
      browsingContext.browserId != this.watcherActor.browserId
    ) {
      return;
    }

    // We need to store the browsing context as this.watcherActor.browserElement.browsingContext
    // can still refer to the previous browsing context at this point.
    this._browsingContext = browsingContext;
    this._updateParentProcessConfiguration(this._getConfiguration());
  },

  _getConfiguration() {
    const targetConfigurationData = this.watcherActor.getWatchedData(
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
    if (!this._browsingContext) {
      return;
    }

    for (const [key, value] of Object.entries(configuration)) {
      switch (key) {
        case "printSimulationEnabled":
          this._setPrintSimulationEnabled(value);
          break;
        case "colorSchemeSimulation":
          this._setColorSchemeSimulation(value);
          break;
        case "serviceWorkersTestingEnabled":
          this._setServiceWorkersTestingEnabled(value);
          break;
      }
    }
  },

  _restoreParentProcessConfiguration() {
    if (!this._browsingContext) {
      return;
    }

    this._setServiceWorkersTestingEnabled(false);
    this._setPrintSimulationEnabled(false);

    // Restore the color scheme simulation only if it was explicitly updated
    // by this actor. This will avoid side effects caused when destroying additional
    // targets (e.g. RDM target, WebExtension target, …).
    // TODO: We may want to review other configuration values to see if we should use
    // the same pattern (Bug 1701553).
    if (this._resetColorSchemeSimulationOnDestroy) {
      this._setColorSchemeSimulation(null);
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
