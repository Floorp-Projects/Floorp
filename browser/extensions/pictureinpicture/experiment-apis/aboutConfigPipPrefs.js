/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

/**
 * Class extending the ExtensionAPI, ensures we can set/get preferences
 */
this.aboutConfigPipPrefs = class extends ExtensionAPI {
  /**
   * Override ExtensionAPI with PiP override's specific preference API, prefixed by `disabled_picture_in_picture_overrides`
   *
   * @param {ExtensionContext} context the context of an extension
   * @returns {object} returns the necessary API structure required to manage prefs within this extension
   */
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;
    const extensionIDBase = context.extension.id.split("@")[0];
    const extensionPrefNameBase = `extensions.${extensionIDBase}.`;

    return {
      aboutConfigPipPrefs: {
        onPrefChange: new EventManager({
          context,
          name: "aboutConfigPipPrefs.onSiteOverridesPrefChange",
          register: (fire, name) => {
            const prefName = `${extensionPrefNameBase}${name}`;
            const callback = () => {
              fire.async(name).catch(() => {}); // ignore Message Manager disconnects
            };
            Services.prefs.addObserver(prefName, callback);
            return () => {
              Services.prefs.removeObserver(prefName, callback);
            };
          },
        }).api(),
        /**
         * Calls `Services.prefs.getBoolPref` to get a preference
         *
         * @param {string} name The name of the preference to get; will be prefixed with this extension's branch
         * @returns {boolean|undefined} the preference, or undefined
         */
        async getPref(name) {
          try {
            return Services.prefs.getBoolPref(
              `${extensionPrefNameBase}${name}`
            );
          } catch (_) {
            return undefined;
          }
        },

        /**
         * Calls `Services.prefs.setBoolPref` to set a preference
         *
         * @param {string} name the name of the preference to set; will be prefixed with this extension's branch
         * @param {boolean} value the bool value to save in the pref
         */
        async setPref(name, value) {
          Services.prefs.setBoolPref(`${extensionPrefNameBase}${name}`, value);
        },
      },
    };
  }
};
