/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* exported ExtensionControlledPopup */

"use strict";

/*
 * @fileOverview
 * This module exports a class that can be used to handle displaying a popup
 * doorhanger with a primary action to not show a popup for this extension again
 * and a secondary action to disable the extension.
 *
 * The original purpose of the popup was to notify users of an extension that has
 * changed the New Tab or homepage. Users would see this popup the first time they
 * view those pages after a change to the setting in each session until they confirm
 * the change by triggering the primary action.
 */

var EXPORTED_SYMBOLS = ["ExtensionControlledPopup"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "CustomizableUI",
                               "resource:///modules/CustomizableUI.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");

let {makeWidgetId} = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "strBundle", function() {
  return Services.strings.createBundle("chrome://global/locale/extensions.properties");
});

const PREF_BRANCH_INSTALLED_ADDON = "extensions.installedDistroAddon.";

XPCOMUtils.defineLazyGetter(this, "distributionAddonsList", function() {
  let addonList = Services.prefs.getChildList(PREF_BRANCH_INSTALLED_ADDON)
                          .map(id => id.replace(PREF_BRANCH_INSTALLED_ADDON, ""));
  return new Set(addonList);
});

class ExtensionControlledPopup {
  /* Provide necessary options for the popup.
   *
   * @param {object} opts Options for configuring popup.
   * @param {string} opts.confirmedType
   *                 The type to use for storing a user's confirmation in
   *                 ExtensionSettingsStore.
   * @param {string} opts.observerTopic
   *                 An observer topic to trigger the popup on with Services.obs. If the
   *                 doorhanger should appear on a specific window include it as the
   *                 subject in the observer event.
   * @param {string} opts.anchorId
   *                 The id to anchor the popupnotification on. If it is not provided
   *                 then it will anchor to a browser action or the app menu.
   * @param {string} opts.popupnotificationId
   *                 The id for the popupnotification element in the markup. This
   *                 element should be defined in panelUI.inc.xul.
   * @param {string} opts.settingType
   *                 The setting type to check in ExtensionSettingsStore to retrieve
   *                 the controlling extension.
   * @param {string} opts.settingKey
   *                 The setting key to check in ExtensionSettingsStore to retrieve
   *                 the controlling extension.
   * @param {string} opts.descriptionId
   *                 The id of the element where the description should be displayed.
   * @param {string} opts.descriptionMessageId
   *                 The message id to be used for the description. The translated
   *                 string will have the add-on's name and icon injected into it.
   * @param {string} opts.getLocalizedDescription
   *                 A function to get the localized message string. This
   *                 function is passed doc, message and addonDetails (the
   *                 add-on's icon and name). If not provided, then the add-on's
   *                 icon and name are added to the description.
   * @param {string} opts.learnMoreMessageId
   *                 The message id to be used for the text of a "learn more" link which
   *                 will be placed after the description.
   * @param {string} opts.learnMoreLink
   *                 The name of the SUMO page to link to, this is added to
   *                 app.support.baseURL.
   * @param {function} opts.onObserverAdded
   *                   A callback that is triggered when an observer is registered to
   *                   trigger the popup on the next observerTopic.
   * @param {function} opts.onObserverRemoved
   *                   A callback that is triggered when the observer is removed,
   *                   either because the popup is opening or it was explicitly
   *                   cancelled by calling removeObserver.
   * @param {function} opts.beforeDisableAddon
   *                   A function that is called before disabling an extension when the
   *                   user decides to disable the extension. If this function is async
   *                   then the extension won't be disabled until it is fulfilled.
   *                   This function gets two arguments, the ExtensionControlledPopup
   *                   instance for the panel and the window that the popup appears on.
   */
  constructor(opts) {
    this.confirmedType = opts.confirmedType;
    this.observerTopic = opts.observerTopic;
    this.anchorId = opts.anchorId;
    this.popupnotificationId = opts.popupnotificationId;
    this.settingType = opts.settingType;
    this.settingKey = opts.settingKey;
    this.descriptionId = opts.descriptionId;
    this.descriptionMessageId = opts.descriptionMessageId;
    this.getLocalizedDescription = opts.getLocalizedDescription;
    this.learnMoreMessageId = opts.learnMoreMessageId;
    this.learnMoreLink = opts.learnMoreLink;
    this.onObserverAdded = opts.onObserverAdded;
    this.onObserverRemoved = opts.onObserverRemoved;
    this.beforeDisableAddon = opts.beforeDisableAddon;
    this.observerRegistered = false;
  }

  get topWindow() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  }

  userHasConfirmed(id) {
    // We don't show a doorhanger for distribution installed add-ons.
    if (distributionAddonsList.has(id)) {
      return true;
    }
    let setting = ExtensionSettingsStore.getSetting(this.confirmedType, id);
    return !!(setting && setting.value);
  }

  async setConfirmation(id) {
    await ExtensionSettingsStore.initialize();
    return ExtensionSettingsStore.addSetting(
      id, this.confirmedType, id, true, () => false);
  }

  async clearConfirmation(id) {
    await ExtensionSettingsStore.initialize();
    return ExtensionSettingsStore.removeSetting(id, this.confirmedType, id);
  }

  observe(subject, topic, data) {
    // Remove the observer here so we don't get multiple open() calls if we get
    // multiple observer events in quick succession.
    this.removeObserver();

    let targetWindow;
    // Some notifications (e.g. browser-open-newtab-start) do not have a window subject.
    if (subject && subject.document) {
      targetWindow = subject;
    }

    // Do this work in an idle callback to avoid interfering with new tab performance tracking.
    this.topWindow.requestIdleCallback(() => this.open(targetWindow));
  }

  removeObserver() {
    if (this.observerRegistered) {
      Services.obs.removeObserver(this, this.observerTopic);
      this.observerRegistered = false;
      if (this.onObserverRemoved) {
        this.onObserverRemoved();
      }
    }
  }

  async addObserver(extensionId) {
    await ExtensionSettingsStore.initialize();

    if (!this.observerRegistered && !this.userHasConfirmed(extensionId)) {
      Services.obs.addObserver(this, this.observerTopic);
      this.observerRegistered = true;
      if (this.onObserverAdded) {
        this.onObserverAdded();
      }
    }
  }

  // The extensionId will be looked up in ExtensionSettingsStore if it is not
  // provided using this.settingType and this.settingKey.
  async open(targetWindow, extensionId) {
    await ExtensionSettingsStore.initialize();

    // Remove the observer since it would open the same dialog again the next time
    // the observer event fires.
    this.removeObserver();

    if (!extensionId) {
      let item = ExtensionSettingsStore.getSetting(
        this.settingType, this.settingKey);
      extensionId = item && item.id;
    }

    // The item should have an extension and the user shouldn't have confirmed
    // the change here, but just to be sure check that it is still controlled
    // and the user hasn't already confirmed the change.
    // If there is no id, then the extension is no longer in control.
    if (!extensionId || this.userHasConfirmed(extensionId)) {
      return;
    }

    // Find the elements we need.
    let win = targetWindow || this.topWindow;
    let doc = win.document;
    let panel = doc.getElementById("extension-notification-panel");
    let popupnotification = doc.getElementById(this.popupnotificationId);
    let urlBarWasFocused = win.gURLBar.focused;

    if (!popupnotification) {
      throw new Error(`No popupnotification found for id "${this.popupnotificationId}"`);
    }

    let addon = await AddonManager.getAddonByID(extensionId);
    this.populateDescription(doc, addon);

    // Setup the command handler.
    let handleCommand = async (event) => {
      panel.hidePopup();
      if (event.originalTarget.getAttribute("anonid") == "button") {
        // Main action is to keep changes.
        await this.setConfirmation(extensionId);
      } else {
        // Secondary action is to restore settings.
        if (this.beforeDisableAddon) {
          await this.beforeDisableAddon(this, win);
        }
        await addon.disable();
      }

      // If the page this is appearing on is the New Tab page then the URL bar may
      // have been focused when the doorhanger stole focus away from it. Once an
      // action is taken the focus state should be restored to what the user was
      // expecting.
      if (urlBarWasFocused) {
        win.gURLBar.focus();
      }
    };
    panel.addEventListener("command", handleCommand);
    panel.addEventListener("popuphidden", () => {
      popupnotification.hidden = true;
      panel.removeEventListener("command", handleCommand);
    }, {once: true});

    let anchorButton;
    if (this.anchorId) {
      // If there's an anchorId, use that right away.
      anchorButton = doc.getElementById(this.anchorId);
    } else {
      // Look for a browserAction on the toolbar.
      let action = CustomizableUI.getWidget(
        `${makeWidgetId(extensionId)}-browser-action`);
      if (action) {
        action = action.areaType == "toolbar" && action.forWindow(win).node;
      }

      // Anchor to a toolbar browserAction if found, otherwise use the menu button.
      anchorButton = action || doc.getElementById("PanelUI-menu-button");
    }
    let anchor = doc.getAnonymousElementByAttribute(
      anchorButton, "class", "toolbarbutton-icon");
    panel.hidden = false;
    popupnotification.hidden = false;
    panel.openPopup(anchor);
  }

  getAddonDetails(doc, addon) {
    const defaultIcon = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

    let image = doc.createElement("image");
    image.setAttribute("src", addon.iconURL || defaultIcon);
    image.classList.add("extension-controlled-icon");

    let addonDetails = doc.createDocumentFragment();
    addonDetails.appendChild(image);
    addonDetails.appendChild(doc.createTextNode(" " + addon.name));

    return addonDetails;
  }

  populateDescription(doc, addon) {
    let description = doc.getElementById(this.descriptionId);
    description.textContent = "";

    let addonDetails = this.getAddonDetails(doc, addon);
    let message = strBundle.GetStringFromName(this.descriptionMessageId);
    if (this.getLocalizedDescription) {
      description.appendChild(
        this.getLocalizedDescription(doc, message, addonDetails));
    } else {
      description.appendChild(
        BrowserUtils.getLocalizedFragment(doc, message, addonDetails));
    }

    let link = doc.createElement("label");
    link.setAttribute("class", "learnMore text-link");
    link.href = Services.urlFormatter.formatURLPref("app.support.baseURL") + this.learnMoreLink;
    link.textContent = strBundle.GetStringFromName(this.learnMoreMessageId);
    description.appendChild(link);
  }
}
