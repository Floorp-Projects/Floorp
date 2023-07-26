/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

this.aboutConfigPrefs = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    return {
      aboutConfigPrefs: {
        onPrefChange: new EventManager({
          context,
          name: "aboutConfigPrefs.onPrefChange",
          register: (fire, name) => {
            const prefName = name;
            const callback = () => {
              fire.async(name).catch(() => {}); // ignore Message Manager disconnects
            };
            Services.prefs.addObserver(prefName, callback);
            return () => {
              Services.prefs.removeObserver(prefName, callback);
            };
          },
        }).api(),
        async getPref(name) {
          try {
            switch (Services.prefs.getPrefType(name)) {
              case 0:
                return undefined;
              case 32:
                return Services.prefs.getStringPref(name);
              case 64:
                return Services.prefs.getIntPref(name);
              case 128:
                return Services.prefs.getBoolPref(name);
            }
          } catch (_) {
            return undefined;
          }
        },
        async getCharPref(name) {
          return Services.prefs.getCharPref(name);
        },
        async getStringPref(name) {
          return Services.prefs.getStringPref(name);
        },
        async getIntPref(name) {
          return Services.prefs.getIntPref(name);
        },
        async getFloatPref(name) {
          return Services.prefs.getFloatPref(name);
        },
        async getBoolPref(name) {
          return Services.prefs.getBoolPref(name);
        },
        async setCharPref(name, value) {
          return Services.prefs.setCharPref(name, value);
        },
        async setStringPref(name, value) {
          return Services.prefs.setStringPref(name, value);
        },
        async setIntPref(name, value) {
          return Services.prefs.setIntPref(name, value);
        },
        async setFloatPref(name, value) {
          return Services.prefs.setFloatPref(name, value);
        },
        async setBoolPref(name, value) {
          return Services.prefs.setBoolPref(name, value);
        },
        async getDefaultPref(name) {
          try {
            switch (Services.prefs.getDefaultBranch(null).getPrefType(name)) {
              case 0:
                return undefined;
              case 32:
                return Services.prefs.getDefaultBranch(null).getStringPref(name);
              case 64:
                return Services.prefs.getDefaultBranch(null).getIntPref(name);
              case 128:
                return Services.prefs.getDefaultBranch(null).getBoolPref(name);
            }
          } catch (_) {
            return undefined;
          }
        },
        async getDefaultCharPref(name) {
          return Services.prefs.getDefaultBranch(null).getCharPref(name);
        },
        async getDefaultStringPref(name) {
          return Services.prefs.getDefaultBranch(null).getStringPref(name);
        },
        async getDefaultIntPref(name) {
          return Services.prefs.getDefaultBranch(null).getIntPref(name);
        },
        async getDefaultFloatPref(name) {
          return Services.prefs.getDefaultBranch(null).getFloatPref(name);
        },
        async getDefaultBoolPref(name) {
          return Services.prefs.getDefaultBranch(null).getBoolPref(name);
        },
        async setDefaultCharPref(name, value) {
          return Services.prefs.getDefaultBranch(null).setCharPref(name, value);
        },
        async setDefaultStringPref(name, value) {
          return Services.prefs.getDefaultBranch(null).setStringPref(name, value);
        },
        async setDefaultIntPref(name, value) {
          return Services.prefs.getDefaultBranch(null).setIntPref(name, value);
        },
        async setDefaultFloatPref(name, value) {
          return Services.prefs.getDefaultBranch(null).setFloatPref(name, value);
        },
        async setDefaultBoolPref(name, value) {
          return Services.prefs.getDefaultBranch(null).setBoolPref(name, value);
        },
        async getPrefType(name) {
          return Services.prefs.getPrefType(name);
        },
        async clearUserPref(name) {
          return Services.prefs.clearUserPref(name);
        },
        async prefHasDefaultValue(name) {
          return Services.prefs.prefHasDefaultValue(name);
        },
        async prefHasUserValue(name) {
          return Services.prefs.prefHasUserValue(name);
        },
        async prefIsLocked(name) {
          return Services.prefs.prefIsLocked(name);
        },
        async lockPref(name) {
          return Services.prefs.lockPref(name);
        },
        async unlockPref(name) {
          return Services.prefs.unlockPref(name);
        },
      },
    };
  }
};
