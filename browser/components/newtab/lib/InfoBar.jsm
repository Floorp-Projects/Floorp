/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  RemoteL10n: "resource://activity-stream/lib/RemoteL10n.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PROTON_ENABLED",
  "browser.proton.enabled",
  false
);

class InfoBarNotification {
  constructor(message, dispatch) {
    this._dispatch = dispatch;
    this.dispatchUserAction = this.dispatchUserAction.bind(this);
    this.buttonCallback = this.buttonCallback.bind(this);
    this.infobarCallback = this.infobarCallback.bind(this);
    this.message = message;
    this.notification = null;
  }

  /**
   * Show the infobar notification and send an impression ping
   *
   * @param {object} browser Browser reference for the currently selected tab
   */
  showNotification(browser) {
    let { content } = this.message;
    let { gBrowser } = browser.ownerGlobal;
    let doc = gBrowser.ownerDocument;
    let notificationContainer;
    if (content.type === "global") {
      notificationContainer = browser.ownerGlobal.gHighPriorityNotificationBox;
    } else {
      notificationContainer = gBrowser.getNotificationBox(browser);
    }

    let priority =
      content.priority ||
      (PROTON_ENABLED
        ? notificationContainer.PRIORITY_SYSTEM
        : notificationContainer.PRIORITY_INFO_MEDIUM);

    this.notification = notificationContainer.appendNotification(
      this.formatMessageConfig(doc, content.text),
      this.message.id,
      content.icon || "chrome://branding/content/icon64.png",
      priority,
      content.buttons.map(b => this.formatButtonConfig(b)),
      this.infobarCallback
    );

    this.addImpression();
  }

  formatMessageConfig(doc, content) {
    let docFragment = doc.createDocumentFragment();
    // notificationbox will only `appendChild` for documentFragments
    docFragment.appendChild(RemoteL10n.createElement(doc, "span", { content }));

    return docFragment;
  }

  formatButtonConfig(button) {
    let btnConfig = { callback: this.buttonCallback, ...button };
    // notificationbox will set correct data-l10n-id attributes if passed in
    // using the l10n-id key. Otherwise the `button.label` text is used.
    if (button.label.string_id) {
      btnConfig["l10n-id"] = button.label.string_id;
    }

    return btnConfig;
  }

  addImpression() {
    // Record an impression in ASRouter for frequency capping
    this._dispatch({ type: "IMPRESSION", data: this.message });
    // Send a user impression telemetry ping
    this.sendUserEventTelemetry("IMPRESSION");
  }

  /**
   * Called when one of the infobar buttons is clicked
   */
  buttonCallback(notificationBox, btnDescription, target) {
    this.dispatchUserAction(
      btnDescription.action,
      target.ownerGlobal.gBrowser.selectedBrowser
    );
    let isPrimary = target.classList.contains("primary");
    let eventName = isPrimary
      ? "CLICK_PRIMARY_BUTTON"
      : "CLICK_SECONDARY_BUTTON";
    this.sendUserEventTelemetry(eventName);
  }

  dispatchUserAction(action, selectedBrowser) {
    this._dispatch({ type: "USER_ACTION", data: action }, selectedBrowser);
  }

  /**
   * Called when interacting with the toolbar (but not through the buttons)
   */
  infobarCallback(eventType) {
    if (eventType === "removed") {
      this.notification = null;
      // eslint-disable-next-line no-use-before-define
      InfoBar._activeInfobar = null;
    } else if (this.notification) {
      this.sendUserEventTelemetry("DISMISSED");
      this.notification = null;
      // eslint-disable-next-line no-use-before-define
      InfoBar._activeInfobar = null;
    }
  }

  sendUserEventTelemetry(event) {
    const ping = {
      message_id: this.message.id,
      event,
    };
    this._dispatch({
      type: "INFOBAR_TELEMETRY",
      data: { action: "infobar_user_event", ...ping },
    });
  }
}

const InfoBar = {
  _activeInfobar: null,

  maybeLoadCustomElement(win) {
    if (!win.customElements.get("remote-text")) {
      Services.scriptloader.loadSubScript(
        "resource://activity-stream/data/custom-elements/paragraph.js",
        win
      );
    }
  },

  maybeInsertFTL(win) {
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
    win.MozXULElement.insertFTLIfNeeded(
      "browser/defaultBrowserNotification.ftl"
    );
  },

  showInfoBarMessage(browser, message, dispatch) {
    // Prevent stacking multiple infobars
    if (this._activeInfobar) {
      return null;
    }

    const win = browser.ownerGlobal;

    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return null;
    }

    this.maybeLoadCustomElement(win);
    this.maybeInsertFTL(win);

    let notification = new InfoBarNotification(message, dispatch);
    notification.showNotification(browser);
    this._activeInfobar = true;

    return notification;
  },
};

this.InfoBar = InfoBar;

const EXPORTED_SYMBOLS = ["InfoBar"];
