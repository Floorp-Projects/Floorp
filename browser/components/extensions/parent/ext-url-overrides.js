/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionControlledPopup",
                               "resource:///modules/ExtensionControlledPopup.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const STORE_TYPE = "url_overrides";
const NEW_TAB_SETTING_NAME = "newTabURL";
const NEW_TAB_CONFIRMED_TYPE = "newTabNotification";

XPCOMUtils.defineLazyGetter(this, "newTabPopup", () => {
  return new ExtensionControlledPopup({
    confirmedType: NEW_TAB_CONFIRMED_TYPE,
    observerTopic: "browser-open-newtab-start",
    popupnotificationId: "extension-new-tab-notification",
    settingType: STORE_TYPE,
    settingKey: NEW_TAB_SETTING_NAME,
    descriptionId: "extension-new-tab-notification-description",
    descriptionMessageId: "newTabControlled.message2",
    learnMoreMessageId: "newTabControlled.learnMore",
    learnMoreLink: "extension-home",
    onObserverAdded() {
      aboutNewTabService.willNotifyUser = true;
    },
    onObserverRemoved() {
      aboutNewTabService.willNotifyUser = false;
    },
    async beforeDisableAddon(popup, win) {
      // ExtensionControlledPopup will disable the add-on once this function completes.
      // Disabling an add-on should remove the tabs that it has open, but we want
      // to open the new New Tab in this tab (which might get closed).
      //   1. Replace the tab's URL with about:blank
      //   2. Return control to ExtensionControlledPopup once about:blank has loaded
      //   3. Once the New Tab URL has changed, replace the tab's URL with the new New Tab URL
      let gBrowser = win.gBrowser;
      let tab = gBrowser.selectedTab;
      await replaceUrlInTab(gBrowser, tab, "about:blank");
      Services.obs.addObserver({
        async observe() {
          await replaceUrlInTab(gBrowser, tab, aboutNewTabService.newTabURL);
          // Now that the New Tab is loading, try to open the popup again. This
          // will only open the popup if a new extension is controlling the New Tab.
          popup.open();
          Services.obs.removeObserver(this, "newtab-url-changed");
        },
      }, "newtab-url-changed");
    },
  });
});

function setNewTabURL(extensionId, url) {
  if (extensionId) {
    newTabPopup.addObserver(extensionId);
  } else {
    newTabPopup.removeObserver();
  }
  aboutNewTabService.newTabURL = url;
}

this.urlOverrides = class extends ExtensionAPI {
  static onUninstall(id) {
    // TODO: This can be removed once bug 1438364 is fixed and all data is cleaned up.
    newTabPopup.clearConfirmation(id);
  }

  processNewTabSetting(action) {
    let {extension} = this;
    let item = ExtensionSettingsStore[action](extension.id, STORE_TYPE, NEW_TAB_SETTING_NAME);
    if (item) {
      setNewTabURL(item.id, item.value || item.initialValue);
    }
  }

  async onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    await ExtensionSettingsStore.initialize();

    if (manifest.chrome_url_overrides.newtab) {
      // Set up the shutdown code for the setting.
      extension.callOnClose({
        close: () => {
          switch (extension.shutdownReason) {
            case "ADDON_DISABLE":
              this.processNewTabSetting("disable");
              newTabPopup.clearConfirmation(extension.id);
              break;

            // We can remove the setting on upgrade or downgrade because it will be
            // added back in when the manifest is re-read. This will cover the case
            // where a new version of an add-on removes the manifest key.
            case "ADDON_DOWNGRADE":
            case "ADDON_UPGRADE":
            case "ADDON_UNINSTALL":
              this.processNewTabSetting("removeSetting");
              break;
          }
        },
      });

      let url = extension.baseURI.resolve(manifest.chrome_url_overrides.newtab);

      let item = await ExtensionSettingsStore.addSetting(
        extension.id, STORE_TYPE, NEW_TAB_SETTING_NAME, url,
        () => aboutNewTabService.newTabURL);

      // If the extension was just re-enabled, change the setting to enabled.
      // This is required because addSetting above is used for both add and update.
      if (["ADDON_ENABLE", "ADDON_UPGRADE", "ADDON_DOWNGRADE"]
          .includes(extension.startupReason)) {
        item = ExtensionSettingsStore.enable(extension.id, STORE_TYPE, NEW_TAB_SETTING_NAME);
      }

      // Set the newTabURL to the current value of the setting.
      if (item) {
        setNewTabURL(item.id, item.value || item.initialValue);
      }
    }
  }
};
