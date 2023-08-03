/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

export const ShoppingUtils = {
  initialized: false,
  registered: false,

  onNimbusUpdate() {
    if (lazy.NimbusFeatures.shopping2023.getVariable("enabled")) {
      ShoppingUtils.init();
    } else {
      ShoppingUtils.uninit();
    }
  },

  // Runs once per session:
  // * at application startup, with startup idle tasks,
  // * or after the user is enrolled in the Nimbus experiment.
  init() {
    if (this.initialized) {
      return;
    }

    if (!this.registered) {
      lazy.NimbusFeatures.shopping2023.onUpdate(ShoppingUtils.onNimbusUpdate);
      this.registered = true;
    }

    if (!lazy.NimbusFeatures.shopping2023.getVariable("enabled")) {
      return;
    }

    // Do startup-time stuff here, like recording startup-time glean events
    // or adjusting onboarding-related prefs once per session.

    this.initialized = true;
  },

  // Runs once per session:
  // * when the user is unenrolled from the Nimbus experiment,
  // * or at shutdown, after quit-application-granted.
  uninit() {
    if (!this.initialized) {
      return;
    }

    // Do shutdown-time stuff here, like firing glean pings or modifying any
    // prefs for onboarding.

    this.initialized = false;
  },
};
