/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ExtensionAPI, ExtensionCommon, Services */

"use strict";

this.telemetryPreferences = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;
    const uploadEnabledPrefName = `datareporting.healthreport.uploadEnabled`;
    const cachedClientIDPrefName = `toolkit.telemetry.cachedClientID`;

    return {
      experiments: {
        telemetryPreferences: {
          onUploadEnabledPrefChange: new EventManager({
            context,
            name: "telemetryPreferences.onUploadEnabledPrefChange",
            register: fire => {
              const callback = () => {
                fire.async().catch(() => {}); // ignore Message Manager disconnects
              };
              Services.prefs.addObserver(uploadEnabledPrefName, callback);
              return () => {
                Services.prefs.removeObserver(uploadEnabledPrefName, callback);
              };
            },
          }).api(),
          async getUploadEnabledPref() {
            return Services.prefs.getBoolPref(uploadEnabledPrefName, undefined);
          },
          onCachedClientIDPrefChange: new EventManager({
            context,
            name: "telemetryPreferences.onCachedClientIDPrefChange",
            register: fire => {
              const callback = () => {
                fire.async().catch(() => {}); // ignore Message Manager disconnects
              };
              Services.prefs.addObserver(cachedClientIDPrefName, callback);
              return () => {
                Services.prefs.removeObserver(cachedClientIDPrefName, callback);
              };
            },
          }).api(),
          async getCachedClientIDPref() {
            return Services.prefs.getStringPref(
              cachedClientIDPrefName,
              undefined,
            );
          },
        },
      },
    };
  }
};
