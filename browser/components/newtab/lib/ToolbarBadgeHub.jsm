/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "EveryWindow",
  "resource:///modules/EveryWindow.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ToolbarPanelHub",
  "resource://activity-stream/lib/ToolbarPanelHub.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "setInterval",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearInterval",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "requestIdleCallback",
  "resource://gre/modules/Timer.jsm"
);

// Frequency at which to check for new messages
const SYSTEM_TICK_INTERVAL = 5 * 60 * 1000;
let notificationsByWindow = new WeakMap();

class _ToolbarBadgeHub {
  constructor() {
    this.id = "toolbar-badge-hub";
    this.state = null;
    this.prefs = {
      WHATSNEW_TOOLBAR_PANEL: "browser.messaging-system.whatsNewPanel.enabled",
      HOMEPAGE_OVERRIDE_PREF: "browser.startup.homepage_override.once",
    };
    this.removeAllNotifications = this.removeAllNotifications.bind(this);
    this.removeToolbarNotification = this.removeToolbarNotification.bind(this);
    this.addToolbarNotification = this.addToolbarNotification.bind(this);
    this.registerBadgeToAllWindows = this.registerBadgeToAllWindows.bind(this);
    this._sendTelemetry = this._sendTelemetry.bind(this);
    this.sendUserEventTelemetry = this.sendUserEventTelemetry.bind(this);
    this.checkHomepageOverridePref = this.checkHomepageOverridePref.bind(this);

    this._handleMessageRequest = null;
    this._addImpression = null;
    this._blockMessageById = null;
    this._dispatch = null;
  }

  async init(
    waitForInitialized,
    {
      handleMessageRequest,
      addImpression,
      blockMessageById,
      unblockMessageById,
      dispatch,
    }
  ) {
    this._handleMessageRequest = handleMessageRequest;
    this._blockMessageById = blockMessageById;
    this._unblockMessageById = unblockMessageById;
    this._addImpression = addImpression;
    this._dispatch = dispatch;
    // Need to wait for ASRouter to initialize before trying to fetch messages
    await waitForInitialized;
    this.messageRequest({
      triggerId: "toolbarBadgeUpdate",
      template: "toolbar_badge",
    });
    // Listen for pref changes that could trigger new badges
    Services.prefs.addObserver(this.prefs.WHATSNEW_TOOLBAR_PANEL, this);
    const _intervalId = setInterval(
      () => this.checkHomepageOverridePref(),
      SYSTEM_TICK_INTERVAL
    );
    this.state = { _intervalId };
  }

  /**
   * Pref is set via Remote Settings message. We want to continously
   * monitor new messages that come in to ensure the one with the
   * highest priority is set.
   */
  checkHomepageOverridePref() {
    const prefValue = Services.prefs.getStringPref(
      this.prefs.HOMEPAGE_OVERRIDE_PREF,
      ""
    );
    if (prefValue) {
      // If the pref is set it means the user has not yet seen this message.
      // We clear the pref value and re-evaluate all possible messages to ensure
      // we don't have a higher priority message to show.
      Services.prefs.clearUserPref(this.prefs.HOMEPAGE_OVERRIDE_PREF);
      let message_id;
      try {
        message_id = JSON.parse(prefValue).message_id;
      } catch (e) {}
      if (message_id) {
        this._unblockMessageById(message_id);
      }
    }

    this.messageRequest({
      triggerId: "momentsUpdate",
      template: "update_action",
    });
  }

  observe(aSubject, aTopic, aPrefName) {
    switch (aPrefName) {
      case this.prefs.WHATSNEW_TOOLBAR_PANEL:
        this.messageRequest({
          triggerId: "toolbarBadgeUpdate",
          template: "toolbar_badge",
        });
        break;
    }
  }

  executeAction({ id, data, message_id }) {
    switch (id) {
      case "show-whatsnew-button":
        ToolbarPanelHub.enableToolbarButton();
        ToolbarPanelHub.enableAppmenuButton();
        break;
      case "moments-wnp":
        const { url, expireDelta } = data;
        let { expire } = data;
        if (!expire) {
          expire = this.getExpirationDate(expireDelta);
        }
        Services.prefs.setStringPref(
          this.prefs.HOMEPAGE_OVERRIDE_PREF,
          JSON.stringify({ message_id, url, expire })
        );
        // Block immediately after taking the action
        this._blockMessageById(message_id);
        break;
    }
  }

  /**
   * If we don't have `expire` defined with the message it could be because
   * it depends on user dependent parameters. Since the message matched
   * targeting we calculate `expire` based on the current timestamp and the
   * `expireDelta` which defines for how long it should be available.
   * @param expireDelta {number} - Offset in milliseconds from the current date
   */
  getExpirationDate(expireDelta) {
    return Date.now() + expireDelta;
  }

  _clearBadgeTimeout() {
    if (this.state.showBadgeTimeoutId) {
      clearTimeout(this.state.showBadgeTimeoutId);
    }
  }

  removeAllNotifications(event) {
    if (event) {
      // ignore right clicks
      if (
        (event.type === "mousedown" || event.type === "click") &&
        event.button !== 0
      ) {
        return;
      }
      // ignore keyboard access that is not one of the usual accessor keys
      if (
        event.type === "keypress" &&
        event.key !== " " &&
        event.key !== "Enter"
      ) {
        return;
      }

      event.target.removeEventListener(
        "mousedown",
        this.removeAllNotifications
      );
      event.target.removeEventListener("click", this.removeAllNotifications);
      // If we have an event it means the user interacted with the badge
      // we should send telemetry
      if (this.state.notification) {
        this.sendUserEventTelemetry("CLICK", this.state.notification);
      }
    }
    // Will call uninit on every window
    EveryWindow.unregisterCallback(this.id);
    if (this.state.notification) {
      this._blockMessageById(this.state.notification.id);
    }
    this._clearBadgeTimeout();
    this.state = {};
  }

  removeToolbarNotification(toolbarButton) {
    // Remove it from the element that displays the badge
    toolbarButton
      .querySelector(".toolbarbutton-badge")
      .classList.remove("feature-callout");
    toolbarButton.removeAttribute("badged");
  }

  addToolbarNotification(win, message) {
    const document = win.browser.ownerDocument;
    if (message.content.action) {
      this.executeAction({ ...message.content.action, message_id: message.id });
    }
    let toolbarbutton = document.getElementById(message.content.target);
    if (toolbarbutton) {
      toolbarbutton.setAttribute("badged", true);
      toolbarbutton
        .querySelector(".toolbarbutton-badge")
        .classList.add("feature-callout");

      // `mousedown` event required because of the `onmousedown` defined on
      // the button that prevents `click` events from firing
      toolbarbutton.addEventListener("mousedown", this.removeAllNotifications);
      // `click` event required for keyboard accessibility
      toolbarbutton.addEventListener("click", this.removeAllNotifications);
      this.state = { notification: { id: message.id } };

      return toolbarbutton;
    }

    return null;
  }

  registerBadgeToAllWindows(message) {
    if (message.template === "update_action") {
      this.executeAction({ ...message.content.action, message_id: message.id });
      // No badge to set only an action to execute
      return;
    }

    // Impression should be added when the badge becomes visible
    this._addImpression(message);
    // Send a telemetry ping when adding the notification badge
    this.sendUserEventTelemetry("IMPRESSION", message);

    EveryWindow.registerCallback(
      this.id,
      win => {
        if (notificationsByWindow.has(win)) {
          // nothing to do
          return;
        }
        const el = this.addToolbarNotification(win, message);
        notificationsByWindow.set(win, el);
      },
      win => {
        const el = notificationsByWindow.get(win);
        if (el) {
          this.removeToolbarNotification(el);
        }
        notificationsByWindow.delete(win);
      }
    );
  }

  registerBadgeNotificationListener(message, options = {}) {
    // We need to clear any existing notifications and only show
    // the one set by devtools
    if (options.force) {
      this.removeAllNotifications();
    }

    if (message.content.delay) {
      this.state.showBadgeTimeoutId = setTimeout(() => {
        requestIdleCallback(() => this.registerBadgeToAllWindows(message));
      }, message.content.delay);
    } else {
      this.registerBadgeToAllWindows(message);
    }
  }

  async messageRequest({ triggerId, template }) {
    const message = await this._handleMessageRequest({
      triggerId,
      template,
    });
    if (message) {
      this.registerBadgeNotificationListener(message);
    }
  }

  _sendTelemetry(ping) {
    this._dispatch({
      type: "TOOLBAR_BADGE_TELEMETRY",
      data: { action: "cfr_user_event", source: "CFR", ...ping },
    });
  }

  sendUserEventTelemetry(event, message) {
    const win = Services.wm.getMostRecentWindow("navigator:browser");
    // Only send pings for non private browsing windows
    if (
      win &&
      !PrivateBrowsingUtils.isBrowserPrivate(
        win.ownerGlobal.gBrowser.selectedBrowser
      )
    ) {
      this._sendTelemetry({
        message_id: message.id,
        bucket_id: message.id,
        event,
      });
    }
  }

  uninit() {
    this._clearBadgeTimeout();
    clearInterval(this.state._intervalId);
    this.state = null;
    notificationsByWindow = new WeakMap();
    Services.prefs.removeObserver(this.prefs.WHATSNEW_TOOLBAR_PANEL, this);
  }
}

this._ToolbarBadgeHub = _ToolbarBadgeHub;

/**
 * ToolbarBadgeHub - singleton instance of _ToolbarBadgeHub that can initiate
 * message requests and render messages.
 */
this.ToolbarBadgeHub = new _ToolbarBadgeHub();

const EXPORTED_SYMBOLS = ["ToolbarBadgeHub", "_ToolbarBadgeHub"];
