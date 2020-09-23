/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This module provides an interface to acces DoH config settings - e.g. whether
 * DoH is enabled, whether capabilities are enabled, etc. Currently this just
 * provides getters for prefs, but imminently will be extended to read config
 * from a Remote Settings collection and filter by client region etc.
 */
var EXPORTED_SYMBOLS = ["Config"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const kEnabledPref = "doh-rollout.enabled";

const kTRRSelectionEnabledPref = "doh-rollout.trr-selection.enabled";
const kTRRSelectionCommitResultPref = "doh-rollout.trr-selection.commit-result";

const kProviderSteeringEnabledPref = "doh-rollout.provider-steering.enabled";
const kProviderSteeringListPref = "doh-rollout.provider-steering.provider-list";

const kPrefChangedTopic = "nsPref:changed";

const Config = {
  init() {
    Preferences.observe(kEnabledPref, this);
  },

  observe(subject, topic, data) {
    switch (topic) {
      case kPrefChangedTopic:
        this.notifyNewConfig();
        break;
    }
  },

  kConfigUpdateTopic: "doh-config-updated",
  notifyNewConfig() {
    Services.obs.notifyObservers(null, this.kConfigUpdateTopic);
  },

  get enabled() {
    return Preferences.get(kEnabledPref, false);
  },

  trrSelection: {
    get enabled() {
      return Preferences.get(kTRRSelectionEnabledPref, false);
    },

    get commitResult() {
      return Preferences.get(kTRRSelectionCommitResultPref, false);
    },
  },

  providerSteering: {
    get enabled() {
      return Preferences.get(kProviderSteeringEnabledPref, false);
    },

    get providerList() {
      return Preferences.get(kProviderSteeringListPref, "[]");
    },
  },
};

Config.init();
