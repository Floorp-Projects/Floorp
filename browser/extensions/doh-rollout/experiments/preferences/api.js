/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);

var preferences = class preferences extends ExtensionAPI {
  getAPI(context) {
    return {
      experiments: {
        preferences: {
          async getIntPref(name, defaultValue) {
            return Services.prefs.getIntPref(name, defaultValue);
          },
          async setIntPref(name, defaultValue) {
            return Services.prefs.setIntPref(name, defaultValue);
          },
          async getBoolPref(name, defaultValue) {
            return Services.prefs.getBoolPref(name, defaultValue);
          },
          async setBoolPref(name, defaultValue) {
            return Services.prefs.setBoolPref(name, defaultValue);
          },
          async getCharPref(name, defaultValue) {
            return Services.prefs.getCharPref(name, defaultValue);
          },
          async setCharPref(name, defaultValue) {
            return Services.prefs.setCharPref(name, defaultValue);
          },
          async clearUserPref(name) {
            return Services.prefs.clearUserPref(name);
          },
          async prefHasUserValue(name) {
            return Services.prefs.prefHasUserValue(name);
          },

          onPrefChanged: new ExtensionCommon.EventManager({
            context,
            name: "preferences.onPrefChanged",
            register: fire => {
              let observer = () => {
                fire.async();
              };
              Services.prefs.addObserver("doh-rollout.enabled", observer);
              Services.prefs.addObserver("doh-rollout.debug", observer);
              return () => {
                Services.prefs.removeObserver("doh-rollout.enabled", observer);
                Services.prefs.removeObserver("doh-rollout.debug", observer);
              };
            },
          }).api(),
        },
      },
    };
  }
};
