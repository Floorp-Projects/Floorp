/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "DeferredTask",
                                  "resource://gre/modules/DeferredTask.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "trackingprotectionUiEnabled",
                                      "privacy.trackingprotection.ui.enabled");

const PREF_SETTING_TYPE = "prefs";
const PROXY_KEY = "proxy.settings";
const API_PROXY_PREFS = [
  "network.proxy.type",
  "network.proxy.http",
  "network.proxy.http_port",
  "network.proxy.share_proxy_settings",
  "network.proxy.ftp",
  "network.proxy.ftp_port",
  "network.proxy.ssl",
  "network.proxy.ssl_port",
  "network.proxy.socks",
  "network.proxy.socks_port",
  "network.proxy.socks_version",
  "network.proxy.socks_remote_dns",
  "network.proxy.no_proxies_on",
  "network.proxy.autoconfig_url",
  "signon.autologin.proxy",
];

let extensionControlledContentIds = {
  "privacy.containers": "browserContainersExtensionContent",
  "homepage_override": "browserHomePageExtensionContent",
  "newTabURL": "browserNewTabExtensionContent",
  "defaultSearch": "browserDefaultSearchExtensionContent",
  "proxy.settings": "proxyExtensionContent",
  get "websites.trackingProtectionMode"() {
    return {
      button: "trackingProtectionExtensionContentButton",
      section:
        trackingprotectionUiEnabled ?
          "trackingProtectionExtensionContentLabel" :
          "trackingProtectionPBMExtensionContentLabel",
    };
  }
};

function getExtensionControlledArgs(settingName) {
  switch (settingName) {
    case "proxyConfig":
      return [document.getElementById("bundleBrand").getString("brandShortName$
    default:
      return [];
  }
}


let extensionControlledIds = {};

/**
  * Check if a pref is being managed by an extension.
  */
async function getControllingExtensionInfo(type, settingName) {
  await ExtensionSettingsStore.initialize();
  return ExtensionSettingsStore.getSetting(type, settingName);
}

function getControllingExtensionEls(settingName) {
  let idInfo = extensionControlledContentIds[settingName];
  let section = document.getElementById(idInfo.section || idInfo);
  let button = idInfo.button ?
    document.getElementById(idInfo.button) :
    section.querySelector("button");
  return {
    section,
    button,
    description: section.querySelector("description"),
  };
}

async function getControllingExtension(type, settingName) {
  let info = await getControllingExtensionInfo(type, settingName);
  let addon = info && info.id
    && await AddonManager.getAddonByID(info.id);
  return addon;
}

async function handleControllingExtension(type, settingName, stringId) {
  let addon = await getControllingExtension(type, settingName);

  // Sometimes the ExtensionSettingsStore gets in a bad state where it thinks
  // an extension is controlling a setting but the extension has been uninstalled
  // outside of the regular lifecycle. If the extension isn't currently installed
  // then we should treat the setting as not being controlled.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1411046 for an example.
  if (addon) {
    extensionControlledIds[settingName] = addon.id;
    showControllingExtension(settingName, addon, stringId);
  } else {
    let elements = getControllingExtensionEls(settingName);
    if (extensionControlledIds[settingName]
        && !document.hidden
        && elements.button) {
      showEnableExtensionMessage(settingName);
    } else {
      hideControllingExtension(settingName);
    }
    delete extensionControlledIds[settingName];
  }

  return !!addon;
}

function getControllingExtensionFragment(stringId, addon, ...extraArgs) {
  let msg = document.getElementById("bundlePreferences").getString(stringId);
  let image = document.createElement("image");
  const defaultIcon = "chrome://mozapps/skin/extensions/extensionGeneric.svg";
  image.setAttribute("src", addon.iconURL || defaultIcon);
  image.classList.add("extension-controlled-icon");
  let addonBit = document.createDocumentFragment();
  addonBit.appendChild(image);
  addonBit.appendChild(document.createTextNode(" " + addon.name));
  return BrowserUtils.getLocalizedFragment(document, msg, addonBit, ...extraArgs);
}

async function showControllingExtension(
  settingName, addon, stringId = `extensionControlled.${settingName}`) {
  // Tell the user what extension is controlling the setting.
  let elements = getControllingExtensionEls(settingName);
  let extraArgs = getExtensionControlledArgs(settingName);

  elements.section.classList.remove("extension-controlled-disabled");
  let description = elements.description;

  // Remove the old content from the description.
  while (description.firstChild) {
    description.firstChild.remove();
  }

  let fragment = getControllingExtensionFragment(
    stringId, addon, ...extraArgs);
  description.appendChild(fragment);

  if (elements.button) {
    elements.button.hidden = false;
  }

  // Show the controlling extension row and hide the old label.
  elements.section.hidden = false;
}

function hideControllingExtension(settingName) {
  let elements = getControllingExtensionEls(settingName);
  elements.section.hidden = true;
  if (elements.button) {
    elements.button.hidden = true;
  }
}

function showEnableExtensionMessage(settingName) {
  let elements = getControllingExtensionEls(settingName);

  elements.button.hidden = true;
  elements.section.classList.add("extension-controlled-disabled");
  let icon = url => {
    let img = document.createElement("image");
    img.src = url;
    img.className = "extension-controlled-icon";
    return img;
  };
  let addonIcon = icon("chrome://mozapps/skin/extensions/extensionGeneric-16.svg");
  let toolbarIcon = icon("chrome://browser/skin/menu.svg");
  let message = document.getElementById("bundlePreferences")
                        .getString("extensionControlled.enable");
  let frag = BrowserUtils.getLocalizedFragment(document, message, addonIcon, toolbarIcon);
  elements.description.innerHTML = "";
  elements.description.appendChild(frag);
  let dismissButton = document.createElement("image");
  dismissButton.setAttribute("class", "extension-controlled-icon close-icon");
  dismissButton.addEventListener("click", function dismissHandler() {
    hideControllingExtension(settingName);
    dismissButton.removeEventListener("click", dismissHandler);
  });
  elements.description.appendChild(dismissButton);
}

function makeDisableControllingExtension(type, settingName) {
  return async function disableExtension() {
    let {id} = await getControllingExtensionInfo(type, settingName);
    let addon = await AddonManager.getAddonByID(id);
    addon.userDisabled = true;
  };
}

function initializeProxyUI(container) {
  let deferredUpdate = new DeferredTask(() => {
    container.updateProxySettingsUI();
  }, 10);
  let proxyObserver = {
    observe: (subject, topic, data) => {
      if (API_PROXY_PREFS.includes(data)) {
        deferredUpdate.arm();
      }
    },
  };
  Services.prefs.addObserver("", proxyObserver);
  window.addEventListener("unload", () => {
    Services.prefs.removeObserver("", proxyObserver);
  });
}
