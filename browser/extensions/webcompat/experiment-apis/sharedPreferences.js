/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, Services, XPCOMUtils */

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  SharedPreferences: "resource://gre/modules/SharedPreferences.jsm",
});

this.sharedPreferences = class extends ExtensionAPI {
  getAPI(context) {
    const extensionIDBase = context.extension.id.split("@")[0];
    const extensionBaseKey = `extensions.${extensionIDBase}`;

    return {
      sharedPreferences: {
        async setBoolPref(name, value) {
          if (!Services.androidBridge || !Services.androidBridge.isFennec) {
            return;
          }
          SharedPreferences.forApp().setBoolPref(
            `${extensionBaseKey}.${value}`,
            value
          );
        },
      },
    };
  }
};
