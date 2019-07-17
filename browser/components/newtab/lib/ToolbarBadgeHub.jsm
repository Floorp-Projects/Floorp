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
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);

const notificationsByWindow = new WeakMap();

class _ToolbarBadgeHub {
  constructor() {
    this.id = "toolbar-badge-hub";
    this.template = "toolbar_badge";
    this.state = null;
    this.removeAllNotifications = this.removeAllNotifications.bind(this);
    this.removeToolbarNotification = this.removeToolbarNotification.bind(this);
    this.addToolbarNotification = this.addToolbarNotification.bind(this);
    this.registerBadgeToAllWindows = this.registerBadgeToAllWindows.bind(this);

    this._handleMessageRequest = null;
    this._addImpression = null;
    this._blockMessageById = null;
  }

  async init(
    waitForInitialized,
    { handleMessageRequest, addImpression, blockMessageById }
  ) {
    this._handleMessageRequest = handleMessageRequest;
    this._blockMessageById = blockMessageById;
    this._addImpression = addImpression;
    this.state = {};
    // Need to wait for ASRouter to initialize before trying to fetch messages
    await waitForInitialized;
    this.messageRequest("toolbarBadgeUpdate");
  }

  executeAction({ id }) {
    switch (id) {
      case "show-whatsnew-button":
        ToolbarPanelHub.enableToolbarButton();
        break;
    }
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
    // Remove it from the toolbar icon
    toolbarButton
      .querySelector(".toolbarbutton-icon")
      .classList.remove("feature-callout");
    toolbarButton.removeAttribute("badged");
  }

  addToolbarNotification(win, message) {
    const document = win.browser.ownerDocument;
    if (message.content.action) {
      this.executeAction(message.content.action);
    }
    let toolbarbutton = document.getElementById(message.content.target);
    if (toolbarbutton) {
      toolbarbutton.setAttribute("badged", true);
      toolbarbutton
        .querySelector(".toolbarbutton-badge")
        .classList.add("feature-callout");
      // This creates the cut-out effect for the icon where the notification
      // fits in
      toolbarbutton
        .querySelector(".toolbarbutton-icon")
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
    // Impression should be added when the badge becomes visible
    this._addImpression(message);

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
        this.removeToolbarNotification(el);
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
        this.registerBadgeToAllWindows(message);
      }, message.content.delay);
    } else {
      this.registerBadgeToAllWindows(message);
    }
  }

  async messageRequest(triggerId) {
    const message = await this._handleMessageRequest({
      triggerId,
      template: this.template,
    });
    if (message) {
      this.registerBadgeNotificationListener(message);
    }
  }

  uninit() {
    this._clearBadgeTimeout();
    this.state = null;
  }
}

this._ToolbarBadgeHub = _ToolbarBadgeHub;

/**
 * ToolbarBadgeHub - singleton instance of _ToolbarBadgeHub that can initiate
 * message requests and render messages.
 */
this.ToolbarBadgeHub = new _ToolbarBadgeHub();

const EXPORTED_SYMBOLS = ["ToolbarBadgeHub", "_ToolbarBadgeHub"];
