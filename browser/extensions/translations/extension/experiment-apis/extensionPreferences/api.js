/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ExtensionAPI, ExtensionCommon, Services */

"use strict";

this.extensionPreferences = class extends ExtensionAPI {
  getAPI() {
    const { Preferences } = ChromeUtils.import(
      "resource://gre/modules/Preferences.jsm",
      {},
    );
    const { ExtensionUtils } = ChromeUtils.import(
      "resource://gre/modules/ExtensionUtils.jsm",
      {},
    );
    const { ExtensionError } = ExtensionUtils;
    const telemetryInactivityThresholdInSecondsOverridePrefName = `extensions.translations.telemetryInactivityThresholdInSecondsOverride`;
    return {
      experiments: {
        extensionPreferences: {
          async getTelemetryInactivityThresholdInSecondsOverridePref() {
            try {
              const value = Preferences.get(
                telemetryInactivityThresholdInSecondsOverridePrefName,
                false,
              );
              if (!value) {
                return false;
              }
              return parseFloat(value);
            } catch (error) {
              // Surface otherwise silent or obscurely reported errors
              console.error(error.message, error.stack);
              throw new ExtensionError(error.message);
            }
          },
        },
      },
    };
  }
};
