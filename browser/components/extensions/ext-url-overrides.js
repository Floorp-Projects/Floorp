/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ext-browser.js */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const STORE_TYPE = "url_overrides";
const NEW_TAB_SETTING_NAME = "newTabURL";
const NEW_TAB_CONFIRMED_TYPE = "newTabNotification";

function userWasNotified(extensionId) {
  let setting = ExtensionSettingsStore.getSetting(NEW_TAB_CONFIRMED_TYPE, extensionId);
  return setting && setting.value;
}

function replaceUrlInTab(gBrowser, tab, url) {
  let loaded = new Promise(resolve => {
    windowTracker.addListener("progress", {
      onLocationChange(browser, webProgress, request, locationURI, flags) {
        if (webProgress.isTopLevel
            && browser.ownerGlobal.gBrowser.getTabForBrowser(browser) == tab
            && locationURI.spec == url) {
          windowTracker.removeListener(this);
          resolve();
        }
      },
    });
  });
  gBrowser.loadURIWithFlags(
    url, {flags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY});
  return loaded;
}

async function handleNewTabOpened() {
  // We don't need to open the doorhanger again until the controlling add-on changes.
  // eslint-disable-next-line no-use-before-define
  removeNewTabObserver();

  let item = ExtensionSettingsStore.getSetting(STORE_TYPE, NEW_TAB_SETTING_NAME);

  if (!item || !item.id || userWasNotified(item.id)) {
    return;
  }

  // Find the elements we need.
  let win = windowTracker.getCurrentWindow({});
  let doc = win.document;
  let panel = doc.getElementById("extension-notification-panel");

  // Setup the command handler.
  let handleCommand = async (event) => {
    if (event.originalTarget.getAttribute("anonid") == "button") {
      // Main action is to keep changes.
      await ExtensionSettingsStore.addSetting(
        item.id, NEW_TAB_CONFIRMED_TYPE, item.id, true, () => false);
    } else {
      // Secondary action is to restore settings. Disabling an add-on should remove
      // the tabs that it has open, but we want to open the new New Tab in this tab.
      //   1. Replace the tab's URL with about:blank, wait for it to change
      //   2. Now that this tab isn't associated with the add-on, disable the add-on
      //   3. Replace the tab's URL with the new New Tab URL
      ExtensionSettingsStore.removeSetting(NEW_TAB_CONFIRMED_TYPE, item.id);
      let addon = await AddonManager.getAddonByID(item.id);
      let gBrowser = win.gBrowser;
      let tab = gBrowser.selectedTab;
      await replaceUrlInTab(gBrowser, tab, "about:blank");
      Services.obs.addObserver({
        async observe() {
          await replaceUrlInTab(gBrowser, tab, aboutNewTabService.newTabURL);
          handleNewTabOpened();
          Services.obs.removeObserver(this, "newtab-url-changed");
        },
      }, "newtab-url-changed");

      addon.userDisabled = true;
    }
    panel.hidePopup();
    win.gURLBar.focus();
  };
  panel.addEventListener("command", handleCommand);
  panel.addEventListener("popuphidden", () => {
    panel.removeEventListener("command", handleCommand);
  }, {once: true});

  // Look for a browserAction on the toolbar.
  let action = CustomizableUI.getWidget(
    `${global.makeWidgetId(item.id)}-browser-action`);
  if (action) {
    action = action.areaType == "toolbar" && action.forWindow(win).node;
  }

  // Anchor to a toolbar browserAction if found, otherwise use the menu button.
  let anchor = doc.getAnonymousElementByAttribute(
    action || doc.getElementById("PanelUI-menu-button"),
    "class", "toolbarbutton-icon");
  panel.hidden = false;
  panel.openPopup(anchor);
}

let newTabOpenedListener = {
  observe(subject, topic, data) {
    // Do this work in an idle callback to avoid interfering with new tab performance tracking.
    windowTracker
      .getCurrentWindow({})
      .requestIdleCallback(handleNewTabOpened);
  },
};

function removeNewTabObserver() {
  if (aboutNewTabService.willNotifyUser) {
    Services.obs.removeObserver(newTabOpenedListener, "browser-open-newtab-start");
    aboutNewTabService.willNotifyUser = false;
  }
}

function addNewTabObserver(extensionId) {
  if (!aboutNewTabService.willNotifyUser && extensionId && !userWasNotified(extensionId)) {
    Services.obs.addObserver(newTabOpenedListener, "browser-open-newtab-start");
    aboutNewTabService.willNotifyUser = true;
  }
}

function setNewTabURL(extensionId, url) {
  if (extensionId) {
    addNewTabObserver(extensionId);
  } else {
    removeNewTabObserver();
  }
  aboutNewTabService.newTabURL = url;
}

this.urlOverrides = class extends ExtensionAPI {
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
          if (extension.shutdownReason == "ADDON_DISABLE"
              || extension.shutdownReason == "ADDON_UNINSTALL") {
            ExtensionSettingsStore.removeSetting(
              extension.id, NEW_TAB_CONFIRMED_TYPE, extension.id);
          }
          switch (extension.shutdownReason) {
            case "ADDON_DISABLE":
              this.processNewTabSetting("disable");
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
