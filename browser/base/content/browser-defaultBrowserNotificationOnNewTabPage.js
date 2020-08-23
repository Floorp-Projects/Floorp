/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
});

var DefaultBrowserNotificationOnNewTabPage = {
  _eventListenerAdded: false,
  async init() {
    // The guards in this function are present to allow
    // tests to re-init between test runs.

    let willPrompt = await this.willCheckDefaultBrowser();
    if (!willPrompt) {
      return;
    }
    if (!this._eventListenerAdded) {
      window.addEventListener("TabSelect", this);
      this._eventListenerAdded = true;
    }
  },

  closePrompt(aNode) {
    if (this._notification) {
      this._notification.close();
    }
  },

  handleEvent(event) {
    if (
      event.type == "TabSelect" &&
      event.target?.linkedBrowser?.currentURI?.spec == AboutNewTab.newTabURL
    ) {
      DefaultBrowserNotificationOnNewTabPage.prompt(event.target.linkedBrowser);
      window.removeEventListener("TabSelect", this);
      this._eventListenerAdded = false;
    }
  },

  prompt(browser) {
    let win = browser.ownerGlobal;
    let { gBrowser } = win;

    let { MozXULElement } = win;
    MozXULElement.insertFTLIfNeeded("browser/defaultBrowserNotification.ftl");
    let doc = browser.ownerDocument;
    let messageFragment = doc.createDocumentFragment();
    let message = doc.createElement("span");
    doc.l10n.setAttributes(message, "default-browser-notification-message");
    messageFragment.appendChild(message);

    let buttons = [
      {
        "l10n-id": "default-browser-notification-button",
        primary: true,
        callback: () => {
          window.getShellService().setAsDefault();
          this.closePrompt();
        },
      },
    ];

    let iconPixels = win.devicePixelRatio > 1 ? "64" : "32";
    let iconURL = "chrome://branding/content/icon" + iconPixels + ".png";
    const priority = win.gNotificationBox.PRIORITY_INFO_MEDIUM;
    let callback = this._onNotificationEvent.bind(this);
    this._notification = gBrowser
      .getNotificationBox(browser)
      .appendNotification(
        messageFragment,
        "default-browser",
        iconURL,
        priority,
        buttons,
        callback
      );
  },

  _onNotificationEvent(eventType) {
    if (eventType == "removed") {
      delete this._notification;
    } else if (eventType == "dismissed") {
      Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", false);
      Services.prefs.setBoolPref(
        "browser.shell.checkDefaultBrowser.disabledByDismissingNotification",
        true
      );
    }
  },

  /**
   * Checks if the default browser check prompt will be shown.
   * @returns {boolean} True if the default browser check prompt will be shown.
   */
  async willCheckDefaultBrowser() {
    // Perform default browser checking.
    const shellService = window.getShellService();
    if (
      !this.enabled ||
      !Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser", false) ||
      AppConstants.DEBUG ||
      !shellService ||
      shellService.isDefaultBrowserOptOut()
    ) {
      return false;
    }

    // Don't show the prompt if we're already the default browser.
    let isDefault = false;
    try {
      isDefault = window.getShellService().isDefaultBrowser(false, false);
    } catch (ex) {}

    if (!isDefault) {
      let promptCount = Services.prefs.getIntPref(
        "browser.defaultbrowser.notificationbar.checkcount",
        0
      );

      promptCount++;
      Services.prefs.setIntPref(
        "browser.defaultbrowser.notificationbar.checkcount",
        promptCount
      );
      if (
        promptCount >
        Services.prefs.getIntPref(
          "browser.defaultbrowser.notificationbar.checklimit"
        )
      ) {
        return false;
      }
    }

    return !isDefault;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  DefaultBrowserNotificationOnNewTabPage,
  "enabled",
  "browser.defaultbrowser.notificationbar",
  false
);
