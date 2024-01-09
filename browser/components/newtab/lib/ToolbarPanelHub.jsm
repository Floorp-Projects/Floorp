/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PanelMultiView: "resource:///modules/PanelMultiView.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  RemoteL10n: "resource://activity-stream/lib/RemoteL10n.sys.mjs",

  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

const idToTextMap = new Map([
  [Ci.nsITrackingDBService.TRACKERS_ID, "trackerCount"],
  [Ci.nsITrackingDBService.TRACKING_COOKIES_ID, "cookieCount"],
  [Ci.nsITrackingDBService.CRYPTOMINERS_ID, "cryptominerCount"],
  [Ci.nsITrackingDBService.FINGERPRINTERS_ID, "fingerprinterCount"],
  [Ci.nsITrackingDBService.SOCIAL_ID, "socialCount"],
]);

const WHATSNEW_ENABLED_PREF = "browser.messaging-system.whatsNewPanel.enabled";
const PROTECTIONS_PANEL_INFOMSG_PREF =
  "browser.protections_panel.infoMessage.seen";

const TOOLBAR_BUTTON_ID = "whats-new-menu-button";
const APPMENU_BUTTON_ID = "appMenu-whatsnew-button";

const BUTTON_STRING_ID = "cfr-whatsnew-button";
const WHATS_NEW_PANEL_SELECTOR = "PanelUI-whatsNew-message-container";

class _ToolbarPanelHub {
  constructor() {
    this.triggerId = "whatsNewPanelOpened";
    this._showAppmenuButton = this._showAppmenuButton.bind(this);
    this._hideAppmenuButton = this._hideAppmenuButton.bind(this);
    this._showToolbarButton = this._showToolbarButton.bind(this);
    this._hideToolbarButton = this._hideToolbarButton.bind(this);

    this.state = {};
    this._initialized = false;
  }

  async init(waitForInitialized, { getMessages, sendTelemetry }) {
    if (this._initialized) {
      return;
    }

    this._initialized = true;
    this._getMessages = getMessages;
    this._sendTelemetry = sendTelemetry;
    // Wait for ASRouter messages to become available in order to know
    // if we can show the What's New panel
    await waitForInitialized;
    // Enable the application menu button so that the user can access
    // the panel outside of the toolbar button
    await this.enableAppmenuButton();

    this.state = {
      protectionPanelMessageSeen: Services.prefs.getBoolPref(
        PROTECTIONS_PANEL_INFOMSG_PREF,
        false
      ),
    };
  }

  uninit() {
    this._initialized = false;
    lazy.EveryWindow.unregisterCallback(TOOLBAR_BUTTON_ID);
    lazy.EveryWindow.unregisterCallback(APPMENU_BUTTON_ID);
  }

  get messages() {
    return this._getMessages({
      template: "whatsnew_panel_message",
      triggerId: "whatsNewPanelOpened",
      returnAll: true,
    });
  }

  toggleWhatsNewPref(event) {
    // Checkbox onclick handler gets called before the checkbox state gets toggled,
    // so we have to call it with the opposite value.
    let newValue = !event.target.checked;
    Services.prefs.setBoolPref(WHATSNEW_ENABLED_PREF, newValue);

    this.sendUserEventTelemetry(
      event.target.ownerGlobal,
      "WNP_PREF_TOGGLE",
      // Message id is not applicable in this case, the notification state
      // is not related to a particular message
      { id: "n/a" },
      { value: { prefValue: newValue } }
    );
  }

  maybeInsertFTL(win) {
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
    win.MozXULElement.insertFTLIfNeeded("toolkit/branding/brandings.ftl");
    win.MozXULElement.insertFTLIfNeeded("toolkit/branding/accounts.ftl");
  }

  maybeLoadCustomElement(win) {
    if (!win.customElements.get("remote-text")) {
      Services.scriptloader.loadSubScript(
        "resource://activity-stream/data/custom-elements/paragraph.js",
        win
      );
    }
  }

  // Turns on the Appmenu (hamburger menu) button for all open windows and future windows.
  async enableAppmenuButton() {
    if ((await this.messages).length) {
      lazy.EveryWindow.registerCallback(
        APPMENU_BUTTON_ID,
        this._showAppmenuButton,
        this._hideAppmenuButton
      );
    }
  }

  // Removes the button from the Appmenu.
  // Only used in tests.
  disableAppmenuButton() {
    lazy.EveryWindow.unregisterCallback(APPMENU_BUTTON_ID);
  }

  // Turns on the Toolbar button for all open windows and future windows.
  async enableToolbarButton() {
    if ((await this.messages).length) {
      lazy.EveryWindow.registerCallback(
        TOOLBAR_BUTTON_ID,
        this._showToolbarButton,
        this._hideToolbarButton
      );
    }
  }

  // When the panel is hidden we want to run some cleanup
  _onPanelHidden(win) {
    const panelContainer = win.document.getElementById(
      "customizationui-widget-panel"
    );
    // When the panel is hidden we want to remove any toolbar buttons that
    // might have been added as an entry point to the panel
    const removeToolbarButton = () => {
      lazy.EveryWindow.unregisterCallback(TOOLBAR_BUTTON_ID);
    };
    if (!panelContainer) {
      return;
    }
    panelContainer.addEventListener("popuphidden", removeToolbarButton, {
      once: true,
    });
  }

  // Newer messages first and use `order` field to decide between messages
  // with the same timestamp
  _sortWhatsNewMessages(m1, m2) {
    // Sort by published_date in descending order.
    if (m1.content.published_date === m2.content.published_date) {
      // Ascending order
      return m1.order - m2.order;
    }
    if (m1.content.published_date > m2.content.published_date) {
      return -1;
    }
    return 1;
  }

  // Render what's new messages into the panel.
  async renderMessages(win, doc, containerId, options = {}) {
    // Set the checked status of the footer checkbox
    let value = Services.prefs.getBoolPref(WHATSNEW_ENABLED_PREF);
    let checkbox = win.document.getElementById("panelMenu-toggleWhatsNew");

    checkbox.checked = value;

    this.maybeLoadCustomElement(win);
    const messages =
      (options.force && options.messages) ||
      (await this.messages).sort(this._sortWhatsNewMessages);
    const container = lazy.PanelMultiView.getViewNode(doc, containerId);

    if (messages) {
      // Targeting attribute state might have changed making new messages
      // available and old messages invalid, we need to refresh
      this.removeMessages(win, containerId);
      let previousDate = 0;
      // Get and store any variable part of the message content
      this.state.contentArguments = await this._contentArguments();
      for (let message of messages) {
        container.appendChild(
          this._createMessageElements(win, doc, message, previousDate)
        );
        previousDate = message.content.published_date;
      }
    }

    this._onPanelHidden(win);

    // Panel impressions are not associated with one particular message
    // but with a set of messages. We concatenate message ids and send them
    // back for every impression.
    const eventId = {
      id: messages
        .map(({ id }) => id)
        .sort()
        .join(","),
    };
    // Check `mainview` attribute to determine if the panel is shown as a
    // subview (inside the application menu) or as a toolbar dropdown.
    // https://searchfox.org/mozilla-central/rev/07f7390618692fa4f2a674a96b9b677df3a13450/browser/components/customizableui/PanelMultiView.jsm#1268
    const mainview = win.PanelUI.whatsNewPanel.hasAttribute("mainview");
    this.sendUserEventTelemetry(win, "IMPRESSION", eventId, {
      value: { view: mainview ? "toolbar_dropdown" : "application_menu" },
    });
  }

  removeMessages(win, containerId) {
    const doc = win.document;
    const messageNodes = lazy.PanelMultiView.getViewNode(
      doc,
      containerId
    ).querySelectorAll(".whatsNew-message");
    for (const messageNode of messageNodes) {
      messageNode.remove();
    }
  }

  /**
   * Dispatch the action defined in the message and user telemetry event.
   */
  _dispatchUserAction(win, message) {
    let url;
    try {
      // Set platform specific path variables for SUMO articles
      url = Services.urlFormatter.formatURL(message.content.cta_url);
    } catch (e) {
      console.error(e);
      url = message.content.cta_url;
    }
    lazy.SpecialMessageActions.handleAction(
      {
        type: message.content.cta_type,
        data: {
          args: url,
          where: message.content.cta_where || "tabshifted",
        },
      },
      win.browser
    );

    this.sendUserEventTelemetry(win, "CLICK", message);
  }

  /**
   * Attach event listener to dispatch message defined action.
   */
  _attachCommandListener(win, element, message) {
    // Add event listener for `mouseup` not to overlap with the
    // `mousedown` & `click` events dispatched from PanelMultiView.sys.mjs
    // https://searchfox.org/mozilla-central/rev/7531325c8660cfa61bf71725f83501028178cbb9/browser/components/customizableui/PanelMultiView.jsm#1830-1837
    element.addEventListener("mouseup", () => {
      this._dispatchUserAction(win, message);
    });
    element.addEventListener("keyup", e => {
      if (e.key === "Enter" || e.key === " ") {
        this._dispatchUserAction(win, message);
      }
    });
  }

  _createMessageElements(win, doc, message, previousDate) {
    const { content } = message;
    const messageEl = lazy.RemoteL10n.createElement(doc, "div");
    messageEl.classList.add("whatsNew-message");

    // Only render date if it is different from the one rendered before.
    if (content.published_date !== previousDate) {
      messageEl.appendChild(
        lazy.RemoteL10n.createElement(doc, "p", {
          classList: "whatsNew-message-date",
          content: new Date(content.published_date).toLocaleDateString(
            "default",
            {
              month: "long",
              day: "numeric",
              year: "numeric",
            }
          ),
        })
      );
    }

    const wrapperEl = lazy.RemoteL10n.createElement(doc, "div");
    wrapperEl.doCommand = () => this._dispatchUserAction(win, message);
    wrapperEl.classList.add("whatsNew-message-body");
    messageEl.appendChild(wrapperEl);

    if (content.icon_url) {
      wrapperEl.classList.add("has-icon");
      const iconEl = lazy.RemoteL10n.createElement(doc, "img");
      iconEl.src = content.icon_url;
      iconEl.classList.add("whatsNew-message-icon");
      if (content.icon_alt && content.icon_alt.string_id) {
        doc.l10n.setAttributes(iconEl, content.icon_alt.string_id);
      } else {
        iconEl.setAttribute("alt", content.icon_alt);
      }
      wrapperEl.appendChild(iconEl);
    }

    wrapperEl.appendChild(this._createMessageContent(win, doc, content));

    if (content.link_text) {
      const anchorEl = lazy.RemoteL10n.createElement(doc, "a", {
        classList: "text-link",
        content: content.link_text,
      });
      anchorEl.doCommand = () => this._dispatchUserAction(win, message);
      wrapperEl.appendChild(anchorEl);
    }

    // Attach event listener on entire message container
    this._attachCommandListener(win, messageEl, message);

    return messageEl;
  }

  /**
   * Return message title (optional subtitle) and body
   */
  _createMessageContent(win, doc, content) {
    const wrapperEl = new win.DocumentFragment();

    wrapperEl.appendChild(
      lazy.RemoteL10n.createElement(doc, "h2", {
        classList: "whatsNew-message-title",
        content: content.title,
        attributes: this.state.contentArguments,
      })
    );

    wrapperEl.appendChild(
      lazy.RemoteL10n.createElement(doc, "p", {
        content: content.body,
        classList: "whatsNew-message-content",
        attributes: this.state.contentArguments,
      })
    );

    return wrapperEl;
  }

  _createHeroElement(win, doc, message) {
    this.maybeLoadCustomElement(win);

    const messageEl = lazy.RemoteL10n.createElement(doc, "div");
    messageEl.setAttribute("id", "protections-popup-message");
    messageEl.classList.add("whatsNew-hero-message");
    const wrapperEl = lazy.RemoteL10n.createElement(doc, "div");
    wrapperEl.classList.add("whatsNew-message-body");
    messageEl.appendChild(wrapperEl);

    wrapperEl.appendChild(
      lazy.RemoteL10n.createElement(doc, "h2", {
        classList: "whatsNew-message-title",
        content: message.content.title,
      })
    );
    wrapperEl.appendChild(
      lazy.RemoteL10n.createElement(doc, "p", {
        classList: "protections-popup-content",
        content: message.content.body,
      })
    );

    if (message.content.link_text) {
      let linkEl = lazy.RemoteL10n.createElement(doc, "a", {
        classList: "text-link",
        content: message.content.link_text,
      });
      linkEl.disabled = true;
      wrapperEl.appendChild(linkEl);
      this._attachCommandListener(win, linkEl, message);
    } else {
      this._attachCommandListener(win, wrapperEl, message);
    }

    return messageEl;
  }

  async _contentArguments() {
    const { defaultEngine } = Services.search;
    // Between now and 6 weeks ago
    const dateTo = new Date();
    const dateFrom = new Date(dateTo.getTime() - 42 * 24 * 60 * 60 * 1000);
    const eventsByDate = await lazy.TrackingDBService.getEventsByDateRange(
      dateFrom,
      dateTo
    );
    // Make sure we set all types of possible values to 0 because they might
    // be referenced by fluent strings
    let totalEvents = { blockedCount: 0 };
    for (let blockedType of idToTextMap.values()) {
      totalEvents[blockedType] = 0;
    }
    // Count all events in the past 6 weeks. Returns an object with:
    // `blockedCount` total number of blocked resources
    // {tracker|cookie|social...} breakdown by event type as defined by `idToTextMap`
    totalEvents = eventsByDate.reduce((acc, day) => {
      const type = day.getResultByName("type");
      const count = day.getResultByName("count");
      acc[idToTextMap.get(type)] = (acc[idToTextMap.get(type)] || 0) + count;
      acc.blockedCount += count;
      return acc;
    }, totalEvents);
    return {
      // Keys need to match variable names used in asrouter.ftl
      // `earliestDate` will be either 6 weeks ago or when tracking recording
      // started. Whichever is more recent.
      earliestDate: Math.max(
        new Date(await lazy.TrackingDBService.getEarliestRecordedDate()),
        dateFrom
      ),
      ...totalEvents,
      // Passing in `undefined` as string for the Fluent variable name
      // in order to match and select the message that does not require
      // the variable.
      searchEngineName: defaultEngine ? defaultEngine.name : "undefined",
    };
  }

  async _showAppmenuButton(win) {
    this.maybeInsertFTL(win);
    await this._showElement(
      win.browser.ownerDocument,
      APPMENU_BUTTON_ID,
      BUTTON_STRING_ID
    );
  }

  _hideAppmenuButton(win, windowClosed) {
    // No need to do something if the window is going away
    if (!windowClosed) {
      this._hideElement(win.browser.ownerDocument, APPMENU_BUTTON_ID);
    }
  }

  _showToolbarButton(win) {
    const document = win.browser.ownerDocument;
    this.maybeInsertFTL(win);
    return this._showElement(document, TOOLBAR_BUTTON_ID, BUTTON_STRING_ID);
  }

  _hideToolbarButton(win) {
    this._hideElement(win.browser.ownerDocument, TOOLBAR_BUTTON_ID);
  }

  _showElement(document, id, string_id) {
    const el = lazy.PanelMultiView.getViewNode(document, id);
    document.l10n.setAttributes(el, string_id);
    el.hidden = false;
  }

  _hideElement(document, id) {
    const el = lazy.PanelMultiView.getViewNode(document, id);
    if (el) {
      el.hidden = true;
    }
  }

  _sendPing(ping) {
    this._sendTelemetry({
      type: "TOOLBAR_PANEL_TELEMETRY",
      data: { action: "whats-new-panel_user_event", ...ping },
    });
  }

  sendUserEventTelemetry(win, event, message, options = {}) {
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
        event_context: options.value,
      });
    }
  }

  /**
   * @param {object} [browser] MessageChannel target argument as a response to a
   *   user action. No message is shown if undefined.
   * @param {object[]} messages Messages selected from devtools page
   */
  forceShowMessage(browser, messages) {
    if (!browser) {
      return;
    }
    const win = browser.ownerGlobal;
    const doc = browser.ownerDocument;
    this.removeMessages(win, WHATS_NEW_PANEL_SELECTOR);
    this.renderMessages(win, doc, WHATS_NEW_PANEL_SELECTOR, {
      force: true,
      messages: Array.isArray(messages) ? messages : [messages],
    });
    win.PanelUI.panel.addEventListener("popuphidden", event =>
      this.removeMessages(event.target.ownerGlobal, WHATS_NEW_PANEL_SELECTOR)
    );
  }
}

/**
 * ToolbarPanelHub - singleton instance of _ToolbarPanelHub that can initiate
 * message requests and render messages.
 */
const ToolbarPanelHub = new _ToolbarPanelHub();

const EXPORTED_SYMBOLS = ["ToolbarPanelHub", "_ToolbarPanelHub"];
