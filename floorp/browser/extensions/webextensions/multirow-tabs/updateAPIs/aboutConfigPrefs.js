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
        async getBranch(branchName) {
          const branch = `${branchName}.`;
          return Services.prefs.getChildList(branch).map(pref => {
            const name = pref.replace(branch, "");
            return { name, value: Services.prefs.getBoolPref(pref) };
          });
        },
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
        async setCharPref(name, value) {
          return Services.prefs.setCharPref(name, value);
        },
        async setIntPref(name, value) {
          return Services.prefs.setIntPref(name, value);
        },
        async setBoolPref(name, value) {
          return Services.prefs.setBoolPref(name, value);
        },
      },
    };
  }
};
