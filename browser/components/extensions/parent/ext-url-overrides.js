/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {ExtensionParent} = ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");

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
const NEW_TAB_PRIVATE_ALLOWED = "browser.newtab.privateAllowed";
const NEW_TAB_EXTENSION_CONTROLLED = "browser.newtab.extensionControlled";

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
    let policy = ExtensionParent.WebExtensionPolicy.getByID(extensionId);
    Services.prefs.setBoolPref(NEW_TAB_PRIVATE_ALLOWED, policy && policy.privateBrowsingAllowed);
    Services.prefs.setBoolPref(NEW_TAB_EXTENSION_CONTROLLED, true);
  } else {
    newTabPopup.removeObserver();
    Services.prefs.clearUserPref(NEW_TAB_PRIVATE_ALLOWED);
    Services.prefs.clearUserPref(NEW_TAB_EXTENSION_CONTROLLED);
  }
  if (url) {
    aboutNewTabService.newTabURL = url;
  }
}

// eslint-disable-next-line mozilla/balanced-listeners
ExtensionParent.apiManager.on("extension-setting-changed", async (eventName, setting) => {
  let extensionId, url;
  if (setting.type === STORE_TYPE && setting.key === NEW_TAB_SETTING_NAME) {
    if (setting.action === "enable" || setting.item) {
      let {item} = setting;
      // If setting.item exists, it is the new value.  If it doesn't exist, and an
      // extension is being enabled, we use the id.
      extensionId = (item && item.id) || setting.id;
      url = item && (item.value || item.initialValue);
    }
  }
  setNewTabURL(extensionId, url);
});

this.urlOverrides = class extends ExtensionAPI {
  static async onDisable(id) {
    newTabPopup.clearConfirmation(id);
    await ExtensionSettingsStore.initialize();
    if (ExtensionSettingsStore.hasSetting(id, STORE_TYPE, NEW_TAB_SETTING_NAME)) {
      ExtensionSettingsStore.disable(id, STORE_TYPE, NEW_TAB_SETTING_NAME);
    }
  }

  static async onUninstall(id) {
    // TODO: This can be removed once bug 1438364 is fixed and all data is cleaned up.
    newTabPopup.clearConfirmation(id);

    await ExtensionSettingsStore.initialize();
    if (ExtensionSettingsStore.hasSetting(id, STORE_TYPE, NEW_TAB_SETTING_NAME)) {
      ExtensionSettingsStore.removeSetting(id, STORE_TYPE, NEW_TAB_SETTING_NAME);
    }
  }

  static async onUpdate(id, manifest) {
    if (!manifest.chrome_url_overrides ||
        !manifest.chrome_url_overrides.newtab) {
      await ExtensionSettingsStore.initialize();
      if (ExtensionSettingsStore.hasSetting(id, STORE_TYPE, NEW_TAB_SETTING_NAME)) {
        ExtensionSettingsStore.removeSetting(id, STORE_TYPE, NEW_TAB_SETTING_NAME);
      }
    }
  }

  async onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    await ExtensionSettingsStore.initialize();

    if (manifest.chrome_url_overrides.newtab) {
      let url = extension.baseURI.resolve(manifest.chrome_url_overrides.newtab);

      let item = await ExtensionSettingsStore.addSetting(
        extension.id, STORE_TYPE, NEW_TAB_SETTING_NAME, url,
        () => aboutNewTabService.newTabURL);

      // Set the newTabURL to the current value of the setting.
      if (item) {
        setNewTabURL(item.id, item.value || item.initialValue);
      }

      // We need to monitor permission change and update the preferences.
      // eslint-disable-next-line mozilla/balanced-listeners
      extension.on("add-permissions", async (ignoreEvent, permissions) => {
        if (permissions.permissions.includes("internal:privateBrowsingAllowed")) {
          let item = await ExtensionSettingsStore.getSetting(STORE_TYPE, NEW_TAB_SETTING_NAME);
          if (item && item.id == extension.id) {
            Services.prefs.setBoolPref(NEW_TAB_PRIVATE_ALLOWED, true);
          }
        }
      });
      // eslint-disable-next-line mozilla/balanced-listeners
      extension.on("remove-permissions", async (ignoreEvent, permissions) => {
        if (permissions.permissions.includes("internal:privateBrowsingAllowed")) {
          let item = await ExtensionSettingsStore.getSetting(STORE_TYPE, NEW_TAB_SETTING_NAME);
          if (item && item.id == extension.id) {
            Services.prefs.setBoolPref(NEW_TAB_PRIVATE_ALLOWED, false);
          }
        }
      });
    }
  }
};
