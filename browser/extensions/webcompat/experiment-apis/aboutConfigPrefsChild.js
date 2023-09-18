/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, Services, XPCOMUtils */

this.aboutConfigPrefs = class extends ExtensionAPI {
  getAPI(context) {
    const extensionIDBase = context.extension.id.split("@")[0];
    const extensionPrefNameBase = `extensions.${extensionIDBase}.`;

    return {
      aboutConfigPrefs: {
        getBoolPrefSync(prefName, defaultValue = false) {
          try {
            return Services.prefs.getBoolPref(
              `${extensionPrefNameBase}${prefName}`,
              defaultValue
            );
          } catch (_) {
            return defaultValue;
          }
        },
      },
    };
  }
};
