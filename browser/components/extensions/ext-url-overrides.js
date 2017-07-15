/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const STORE_TYPE = "url_overrides";
const NEW_TAB_SETTING_NAME = "newTabURL";

this.urlOverrides = class extends ExtensionAPI {
  processNewTabSetting(action) {
    let {extension} = this;
    let item = ExtensionSettingsStore[action](extension, STORE_TYPE, NEW_TAB_SETTING_NAME);
    if (item) {
      aboutNewTabService.newTabURL = item.value || item.initialValue;
    }
  }

  async onStartup() {
    let {extension} = this;
    let {manifest} = extension;

    // Record the setting if it exists in the manifest.
    if (manifest.chrome_url_overrides && manifest.chrome_url_overrides.newtab) {
      let url = extension.baseURI.resolve(manifest.chrome_url_overrides.newtab);

      let item = await ExtensionSettingsStore.addSetting(
        extension, STORE_TYPE, NEW_TAB_SETTING_NAME, url,
        () => aboutNewTabService.newTabURL);

      // If the extension was just re-enabled, change the setting to enabled.
      // This is required because addSetting above is used for both add and update.
      if (["ADDON_ENABLE", "ADDON_UPGRADE", "ADDON_DOWNGRADE"]
          .includes(extension.startupReason)) {
        item = ExtensionSettingsStore.enable(extension, STORE_TYPE, NEW_TAB_SETTING_NAME);
      }

      // Set the newTabURL to the current value of the setting.
      if (item) {
        aboutNewTabService.newTabURL = item.value || item.initialValue;
      }
    // If the setting exists for the extension, but is missing from the manifest,
    // remove it.
    } else if (ExtensionSettingsStore.hasSetting(
               extension, STORE_TYPE, NEW_TAB_SETTING_NAME)) {
      this.processNewTabSetting("removeSetting");
    }
  }

  onShutdown(shutdownReason) {
    switch (shutdownReason) {
      case "ADDON_DISABLE":
      case "ADDON_DOWNGRADE":
      case "ADDON_UPGRADE":
        this.processNewTabSetting("disable");
        break;

      case "ADDON_UNINSTALL":
        this.processNewTabSetting("removeSetting");
        break;
    }
  }
};
