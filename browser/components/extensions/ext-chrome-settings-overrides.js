/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPreferencesManager",
                                  "resource://gre/modules/ExtensionPreferencesManager.jsm");

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_chrome_settings_overrides", (type, directive, extension, manifest) => {
  if (manifest.chrome_settings_overrides.homepage) {
    ExtensionPreferencesManager.setSetting(extension, "homepage_override",
                                           manifest.chrome_settings_overrides.homepage);
  }
});
/* eslint-enable mozilla/balanced-listeners */

ExtensionPreferencesManager.addSetting("homepage_override", {
  prefNames: [
    "browser.startup.homepage",
  ],
  setCallback(value) {
    return {
      "browser.startup.homepage": value,
    };
  },
});
