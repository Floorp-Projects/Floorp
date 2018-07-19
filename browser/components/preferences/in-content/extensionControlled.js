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
      section: "trackingProtectionExtensionContentLabel",
    };
  }
};

const extensionControlledL10nKeys = {
  "homepage_override": "homepage-override",
  "newTabURL": "new-tab-url",
  "defaultSearch": "default-search",
  "privacy.containers": "privacy-containers",
  "websites.trackingProtectionMode": "websites-tracking-protection-mode",
  "proxy.settings": "proxy-config",
};

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

async function handleControllingExtension(type, settingName) {
  let addon = await getControllingExtension(type, settingName);

  // Sometimes the ExtensionSettingsStore gets in a bad state where it thinks
  // an extension is controlling a setting but the extension has been uninstalled
  // outside of the regular lifecycle. If the extension isn't currently installed
  // then we should treat the setting as not being controlled.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1411046 for an example.
  if (addon) {
    extensionControlledIds[settingName] = addon.id;
    showControllingExtension(settingName, addon);
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

function settingNameToL10nID(settingName) {
  if (!extensionControlledL10nKeys.hasOwnProperty(settingName)) {
    throw new Error(`Unknown extension controlled setting name: ${settingName}`);
  }
  return `extension-controlled-${extensionControlledL10nKeys[settingName]}`;
}


/**
 * Set the localization data for the description of the controlling extension.
 *
 * @param elem {Element}
 *        <description> element to be annotated
 * @param addon {Object?}
 *        Addon object with meta information about the addon (or null)
 * @param settingName {String}
 *        If `addon` is set this handled the name of the setting that will be used
 *        to fetch the l10n id for the given message.
 *        If `addon` is set to null, this will be the full l10n-id assigned to the
 *        element.
 */
function setControllingExtensionDescription(elem, addon, settingName) {
  // Remove the old content from the description.
  while (elem.firstChild) {
    elem.firstChild.remove();
  }

  if (addon === null) {
    document.l10n.setAttributes(elem, settingName);
    return;
  }

  let image = document.createElementNS("http://www.w3.org/1999/xhtml", "img");
  const defaultIcon = "chrome://mozapps/skin/extensions/extensionGeneric.svg";
  image.setAttribute("src", addon.iconURL || defaultIcon);
  image.setAttribute("data-l10n-name", "icon");
  image.classList.add("extension-controlled-icon");
  elem.appendChild(image);
  const l10nId = settingNameToL10nID(settingName);
  document.l10n.setAttributes(elem, l10nId, {
    name: addon.name
  });
}

async function showControllingExtension(settingName, addon) {
  // Tell the user what extension is controlling the setting.
  let elements = getControllingExtensionEls(settingName);

  elements.section.classList.remove("extension-controlled-disabled");
  let description = elements.description;

  setControllingExtensionDescription(description, addon, settingName);

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

  elements.description.textContent = "";

  // We replace localization of the <description> with a DOM Fragment containing
  // the enable-extension-enable message. That means a change from:
  //
  // <description data-l10n-id="..."/>
  //
  // to:
  //
  // <description>
  //   <img/>
  //   <label data-l10n-id="..."/>
  // </description>
  //
  // We need to remove the l10n-id annotation from the <description> to prevent
  // Fluent from overwriting the element in case of any retranslation.
  elements.description.removeAttribute("data-l10n-id");

  let icon = (url, name) => {
    let img = document.createElementNS("http://www.w3.org/1999/xhtml", "img");
    img.src = url;
    img.setAttribute("data-l10n-name", name);
    img.className = "extension-controlled-icon";
    return img;
  };
  let label = document.createElement("label");
  let addonIcon = icon("chrome://mozapps/skin/extensions/extensionGeneric-16.svg", "addons-icon");
  let toolbarIcon = icon("chrome://browser/skin/menu.svg", "menu-icon");
  label.appendChild(addonIcon);
  label.appendChild(toolbarIcon);
  document.l10n.setAttributes(label, "extension-controlled-enable");
  elements.description.appendChild(label);
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
    await addon.disable();
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
