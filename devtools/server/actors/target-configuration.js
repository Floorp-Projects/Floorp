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
 *
 * @constructor
 *
 */
const TargetConfigurationActor = ActorClassWithSpec(targetConfigurationSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);
  },

  form() {
    return {
      actor: this.actorID,
      configuration: this._getConfiguration(),
      traits: { supportedOptions: SUPPORTED_OPTIONS },
    };
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

    await this.watcherActor.addDataEntry(TARGET_CONFIGURATION, cfgArray);
    return this._getConfiguration();
  },
});

exports.TargetConfigurationActor = TargetConfigurationActor;
