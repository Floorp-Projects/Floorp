/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.aboutConfigPrefs = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;
    const extensionIDBase = context.extension.id.split("@")[0];
    const endpointPrefName = `extensions.${extensionIDBase}.newIssueEndpoint`;

    return {
      aboutConfigPrefs: {
        onEndpointPrefChange: new EventManager({
          context,
          name: "aboutConfigPrefs.onEndpointPrefChange",
          register: fire => {
            const callback = () => {
              fire.async().catch(() => {}); // ignore Message Manager disconnects
            };
            Services.prefs.addObserver(endpointPrefName, callback);
            return () => {
              Services.prefs.removeObserver(endpointPrefName, callback);
            };
          },
        }).api(),
        async getEndpointPref() {
          return Services.prefs.getStringPref(endpointPrefName, undefined);
        },
        async setEndpointPref(value) {
          Services.prefs.setStringPref(endpointPrefName, value);
        },
      },
    };
  }
};
