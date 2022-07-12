/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.jsm",
  ToolbarPanelHub: "resource://activity-stream/lib/ToolbarPanelHub.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  requestIdleCallback: "resource://gre/modules/Timer.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
});

let notificationsByWindow = new WeakMap();

class _ToolbarBadgeHub {
  constructor() {
    this.id = "toolbar-badge-hub";
    this.state = {};
    this.prefs = {
      WHATSNEW_TOOLBAR_PANEL: "browser.messaging-system.whatsNewPanel.enabled",
    };
    this.removeAllNotifications = this.removeAllNotifications.bind(this);
    this.removeToolbarNotification = this.removeToolbarNotification.bind(this);
    this.addToolbarNotification = this.addToolbarNotification.bind(this);
    this.registerBadgeToAllWindows = this.registerBadgeToAllWindows.bind(this);
    this._sendPing = this._sendPing.bind(this);
    this.sendUserEventTelemetry = this.sendUserEventTelemetry.bind(this);

    this._handleMessageRequest = null;
    this._addImpression = null;
    this._blockMessageById = null;
    this._sendTelemetry = null;
    this._initialized = false;
  }

  async init(
    waitForInitialized,
    {
      handleMessageRequest,
      addImpression,
      blockMessageById,
      unblockMessageById,
      sendTelemetry,
    }
  ) {
    if (this._initialized) {
      return;
    }

    this._initialized = true;
    this._handleMessageRequest = handleMessageRequest;
    this._blockMessageById = blockMessageById;
    this._unblockMessageById = unblockMessageById;
    this._addImpression = addImpression;
    this._sendTelemetry = sendTelemetry;
    // Need to wait for ASRouter to initialize before trying to fetch messages
    await waitForInitialized;
    this.messageRequest({
      triggerId: "toolbarBadgeUpdate",
      template: "toolbar_badge",
    });
    // Listen for pref changes that could trigger new badges
    Services.prefs.addObserver(this.prefs.WHATSNEW_TOOLBAR_PANEL, this);
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

  maybeInsertFTL(win) {
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
  }

  executeAction({ id, data, message_id }) {
    switch (id) {
      case "show-whatsnew-button":
        lazy.ToolbarPanelHub.enableToolbarButton();
        lazy.ToolbarPanelHub.enableAppmenuButton();
        break;
    }
  }

  _clearBadgeTimeout() {
    if (this.state.showBadgeTimeoutId) {
      lazy.clearTimeout(this.state.showBadgeTimeoutId);
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
      event.target.removeEventListener("keypress", this.removeAllNotifications);
      // If we have an event it means the user interacted with the badge
      // we should send telemetry
      if (this.state.notification) {
        this.sendUserEventTelemetry("CLICK", this.state.notification);
      }
    }
    // Will call uninit on every window
    lazy.EveryWindow.unregisterCallback(this.id);
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
    // Remove id used for for aria-label badge description
    const notificationDescription = toolbarButton.querySelector(
      "#toolbarbutton-notification-description"
    );
    if (notificationDescription) {
      notificationDescription.remove();
      toolbarButton.removeAttribute("aria-labelledby");
      toolbarButton.removeAttribute("aria-describedby");
    }
  }

  addToolbarNotification(win, message) {
    const document = win.browser.ownerDocument;
    if (message.content.action) {
      this.executeAction({ ...message.content.action, message_id: message.id });
    }
    let toolbarbutton = document.getElementById(message.content.target);
    if (toolbarbutton) {
      const badge = toolbarbutton.querySelector(".toolbarbutton-badge");
      badge.classList.add("feature-callout");
      toolbarbutton.setAttribute("badged", true);
      // If we have additional aria-label information for the notification
      // we add this content to the hidden `toolbarbutton-text` node.
      // We then use `aria-labelledby` to link this description to the button
      // that received the notification badge.
      if (message.content.badgeDescription) {
        // Insert strings as soon as we know we're showing them
        this.maybeInsertFTL(win);
        toolbarbutton.setAttribute(
          "aria-labelledby",
          `toolbarbutton-notification-description ${message.content.target}`
        );
        // Because tooltiptext is different to the label, it gets duplicated as
        // the description. Setting `describedby` to the same value as
        // `labelledby` will be detected by the a11y code and the description
        // will be removed.
        toolbarbutton.setAttribute(
          "aria-describedby",
          `toolbarbutton-notification-description ${message.content.target}`
        );
        const descriptionEl = document.createElement("span");
        descriptionEl.setAttribute(
          "id",
          "toolbarbutton-notification-description"
        );
        descriptionEl.hidden = true;
        document.l10n.setAttributes(
          descriptionEl,
          message.content.badgeDescription.string_id
        );
        toolbarbutton.appendChild(descriptionEl);
      }
      // `mousedown` event required because of the `onmousedown` defined on
      // the button that prevents `click` events from firing
      toolbarbutton.addEventListener("mousedown", this.removeAllNotifications);
      // `keypress` event required for keyboard accessibility
      toolbarbutton.addEventListener("keypress", this.removeAllNotifications);
      this.state = { notification: { id: message.id } };

      // Impression should be added when the badge becomes visible
      this._addImpression(message);
      // Send a telemetry ping when adding the notification badge
      this.sendUserEventTelemetry("IMPRESSION", message);

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

    lazy.EveryWindow.registerCallback(
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
      // When debugging immediately show the badge
      this.registerBadgeToAllWindows(message);
      return;
    }

    if (message.content.delay) {
      this.state.showBadgeTimeoutId = lazy.setTimeout(() => {
        lazy.requestIdleCallback(() => this.registerBadgeToAllWindows(message));
      }, message.content.delay);
    } else {
      this.registerBadgeToAllWindows(message);
    }
  }

  async messageRequest({ triggerId, template }) {
    const telemetryObject = { triggerId };
    TelemetryStopwatch.start("MS_MESSAGE_REQUEST_TIME_MS", telemetryObject);
    const message = await this._handleMessageRequest({
      triggerId,
      template,
    });
    TelemetryStopwatch.finish("MS_MESSAGE_REQUEST_TIME_MS", telemetryObject);
    if (message) {
      this.registerBadgeNotificationListener(message);
    }
  }

  _sendPing(ping) {
    this._sendTelemetry({
      type: "TOOLBAR_BADGE_TELEMETRY",
      data: { action: "badge_user_event", ...ping },
    });
  }

  sendUserEventTelemetry(event, message) {
    const win = Services.wm.getMostRecentWindow("navigator:browser");
    // Only send pings for non private browsing windows
    if (
      win &&
      !lazy.PrivateBrowsingUtils.isBrowserPrivate(
        win.ownerGlobal.gBrowser.selectedBrowser
      )
    ) {
      this._sendPing({
        message_id: message.id,
        event,
      });
    }
  }

  uninit() {
    this._clearBadgeTimeout();
    this.state = {};
    this._initialized = false;
    notificationsByWindow = new WeakMap();
    Services.prefs.removeObserver(this.prefs.WHATSNEW_TOOLBAR_PANEL, this);
  }
}

/**
 * ToolbarBadgeHub - singleton instance of _ToolbarBadgeHub that can initiate
 * message requests and render messages.
 */
const ToolbarBadgeHub = new _ToolbarBadgeHub();

const EXPORTED_SYMBOLS = ["ToolbarBadgeHub", "_ToolbarBadgeHub"];
