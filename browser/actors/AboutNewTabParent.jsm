/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutNewTabParent", "DefaultBrowserNotification"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "DEFAULT_BROWSER_NOTIFICATION_ENABLED",
  "browser.defaultbrowser.notificationbar",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SHOULD_CHECK_DEFAULT_BROWSER",
  "browser.shell.checkDefaultBrowser",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PROTON_ENABLED",
  "browser.proton.enabled",
  false
);

class AboutNewTabParent extends JSWindowActorParent {
  async receiveMessage(message) {
    switch (message.name) {
      case "DefaultBrowserNotification":
        ASRouter.waitForInitialized.then(() =>
          ASRouter.sendTriggerMessage({
            browser: this.browsingContext.top.embedderElement,
            // triggerId and triggerContext
            id: "defaultBrowserCheck",
            context: { source: "newtab" },
          })
        );
        await DefaultBrowserNotification.maybeShow(
          this.browsingContext.top.embedderElement,
          this.browsingContext.topChromeWindow?.getShellService()
        );
    }
  }
}

// This is a singleton so we can limit display of the notification to
// once per session.
var DefaultBrowserNotification = {
  _notificationDisplayed: false,
  _modalPromptDisplayed: false,
  _notification: null,

  // Used by tests to reset the internal state
  reset() {
    this._notificationDisplayed = false;
    this._modalPromptDisplayed = false;
    this._notification = null;
  },

  notifyModalDisplayed() {
    this._modalPromptDisplayed = true;
  },

  // ShellService is injected here so tests can fake it
  async maybeShow(browser, shellService) {
    if (
      !this._notificationDisplayed &&
      !this._modalPromptDisplayed &&
      !this._notification
    ) {
      let shouldPrompt = await this.willCheckDefaultBrowser(shellService);
      if (shouldPrompt) {
        this.prompt(browser);
        this._notificationDisplayed = true;
      }
    }
  },

  /**
   * Checks if the default browser check prompt will be shown.
   * @returns {boolean} True if the default browser check prompt will be shown.
   */
  async willCheckDefaultBrowser(shellService) {
    if (
      !DEFAULT_BROWSER_NOTIFICATION_ENABLED ||
      !SHOULD_CHECK_DEFAULT_BROWSER ||
      AppConstants.DEBUG ||
      !shellService ||
      shellService.isDefaultBrowserOptOut()
    ) {
      return false;
    }

    // Don't show the prompt if we're already the default browser.
    let isDefault = false;
    try {
      isDefault = shellService.isDefaultBrowser(false, false);
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
          win.getShellService().setAsDefault();
          this.closeNotification();
        },
      },
    ];

    let iconPixels = win.devicePixelRatio > 1 ? "64" : "32";
    let iconURL = "chrome://branding/content/icon" + iconPixels + ".png";
    const priority = PROTON_ENABLED
      ? win.gNotificationBox.PRIORITY_SYSTEM
      : win.gNotificationBox.PRIORITY_INFO_MEDIUM;
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
      this._notification = null;
    } else if (eventType == "dismissed") {
      Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", false);
      Services.prefs.setBoolPref(
        "browser.shell.checkDefaultBrowser.disabledByDismissingNotification",
        true
      );
    }
  },

  closeNotification() {
    if (this._notification) {
      this._notification.close();
      this._notification = null;
    }
  },
};
