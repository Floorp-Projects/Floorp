/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  EveryWindow: "resource:///modules/EveryWindow.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});
XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

const WHATSNEW_ENABLED_PREF = "browser.messaging-system.whatsNewPanel.enabled";
const PROTECTIONS_PANEL_INFOMSG_PREF =
  "browser.protections_panel.infoMessage.seen";

const TOOLBAR_BUTTON_ID = "whats-new-menu-button";
const APPMENU_BUTTON_ID = "appMenu-whatsnew-button";
const PANEL_HEADER_SELECTOR = "#PanelUI-whatsNew-title > label";

const BUTTON_STRING_ID = "cfr-whatsnew-button";
const WHATS_NEW_PANEL_SELECTOR = "PanelUI-whatsNew-message-container";

class _ToolbarPanelHub {
  constructor() {
    this.triggerId = "whatsNewPanelOpened";
    this._showAppmenuButton = this._showAppmenuButton.bind(this);
    this._hideAppmenuButton = this._hideAppmenuButton.bind(this);
    this._showToolbarButton = this._showToolbarButton.bind(this);
    this._hideToolbarButton = this._hideToolbarButton.bind(this);
    this.insertProtectionPanelMessage = this.insertProtectionPanelMessage.bind(
      this
    );

    this.state = null;
  }

  async init(waitForInitialized, { getMessages, dispatch, handleUserAction }) {
    this._getMessages = getMessages;
    this._dispatch = dispatch;
    this._handleUserAction = handleUserAction;
    // Wait for ASRouter messages to become available in order to know
    // if we can show the What's New panel
    await waitForInitialized;
    if (this.whatsNewPanelEnabled) {
      // Enable the application menu button so that the user can access
      // the panel outside of the toolbar button
      this.enableAppmenuButton();
    }
    // Listen for pref changes that could turn off the feature
    Services.prefs.addObserver(WHATSNEW_ENABLED_PREF, this);

    this.state = {
      protectionPanelMessageSeen: Services.prefs.getBoolPref(
        PROTECTIONS_PANEL_INFOMSG_PREF,
        false
      ),
    };
  }

  uninit() {
    EveryWindow.unregisterCallback(TOOLBAR_BUTTON_ID);
    EveryWindow.unregisterCallback(APPMENU_BUTTON_ID);
    Services.prefs.removeObserver(WHATSNEW_ENABLED_PREF, this);
  }

  observe(aSubject, aTopic, aPrefName) {
    switch (aPrefName) {
      case WHATSNEW_ENABLED_PREF:
        if (!this.whatsNewPanelEnabled) {
          this.uninit();
        }
        break;
    }
  }

  get messages() {
    return this._getMessages({
      template: "whatsnew_panel_message",
      triggerId: "whatsNewPanelOpened",
      returnAll: true,
    });
  }

  get whatsNewPanelEnabled() {
    return Services.prefs.getBoolPref(WHATSNEW_ENABLED_PREF, false);
  }

  maybeInsertFTL(win) {
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
    win.MozXULElement.insertFTLIfNeeded("browser/branding/brandings.ftl");
    win.MozXULElement.insertFTLIfNeeded("browser/branding/sync-brand.ftl");
  }

  // Turns on the Appmenu (hamburger menu) button for all open windows and future windows.
  async enableAppmenuButton() {
    if ((await this.messages).length) {
      EveryWindow.registerCallback(
        APPMENU_BUTTON_ID,
        this._showAppmenuButton,
        this._hideAppmenuButton
      );
    }
  }

  // Turns on the Toolbar button for all open windows and future windows.
  async enableToolbarButton() {
    if ((await this.messages).length) {
      EveryWindow.registerCallback(
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
      EveryWindow.unregisterCallback(TOOLBAR_BUTTON_ID);
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
    const messages =
      (options.force && options.messages) ||
      (await this.messages).sort(this._sortWhatsNewMessages);
    const container = doc.getElementById(containerId);

    if (messages && !container.querySelector(".whatsNew-message")) {
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
    const messageNodes = doc
      .getElementById(containerId)
      .querySelectorAll(".whatsNew-message");
    for (const messageNode of messageNodes) {
      messageNode.remove();
    }
  }

  /**
   * Attach click event listener defined in message payload
   */
  _attachClickListener(win, element, message) {
    element.addEventListener("click", () => {
      this._handleUserAction({
        target: win,
        data: {
          type: message.content.cta_type,
          data: {
            args: message.content.cta_url,
            where: "tabshifted",
          },
        },
      });

      this.sendUserEventTelemetry(win, "CLICK", message);
    });
  }

  _createMessageElements(win, doc, message, previousDate) {
    const { content } = message;
    const messageEl = this._createElement(doc, "div");
    messageEl.classList.add("whatsNew-message");

    // Only render date if it is different from the one rendered before.
    if (content.published_date !== previousDate) {
      messageEl.appendChild(
        this._createElement(doc, "p", {
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

    const wrapperEl = this._createElement(doc, "button");
    // istanbul ignore next
    wrapperEl.doCommand = () => {};
    wrapperEl.classList.add("whatsNew-message-body");
    messageEl.appendChild(wrapperEl);

    if (content.icon_url) {
      wrapperEl.classList.add("has-icon");
      const iconEl = this._createElement(doc, "img");
      iconEl.src = content.icon_url;
      iconEl.classList.add("whatsNew-message-icon");
      this._setTextAttribute(doc, iconEl, "alt", content.icon_alt);
      wrapperEl.appendChild(iconEl);
    }

    wrapperEl.appendChild(this._createMessageContent(win, doc, content));

    if (content.link_text) {
      const anchorEl = this._createElement(doc, "a", {
        classList: "text-link",
        content: content.link_text,
      });
      // istanbul ignore next
      anchorEl.doCommand = () => {};
      wrapperEl.appendChild(anchorEl);
    }

    // Attach event listener on entire message container
    this._attachClickListener(win, messageEl, message);

    return messageEl;
  }

  /**
   * Return message title (optional subtitle) and body
   */
  _createMessageContent(win, doc, content) {
    const wrapperEl = new win.DocumentFragment();

    wrapperEl.appendChild(
      this._createElement(doc, "h2", {
        classList: "whatsNew-message-title",
        content: content.title,
      })
    );

    switch (content.layout) {
      case "tracking-protections":
        wrapperEl.appendChild(
          this._createElement(doc, "h4", {
            classList: "whatsNew-message-subtitle",
            content: content.subtitle,
          })
        );
        wrapperEl.appendChild(
          this._createElement(doc, "h2", {
            classList: "whatsNew-message-title-large",
            content: this.state.contentArguments.blockedCount,
          })
        );
        break;
    }

    wrapperEl.appendChild(
      this._createElement(doc, "p", { content: content.body })
    );

    return wrapperEl;
  }

  _createHeroElement(win, doc, message) {
    const messageEl = this._createElement(doc, "div");
    messageEl.setAttribute("id", "protections-popup-message");
    messageEl.classList.add("whatsNew-hero-message");
    const wrapperEl = this._createElement(doc, "div");
    wrapperEl.classList.add("whatsNew-message-body");
    messageEl.appendChild(wrapperEl);

    this._attachClickListener(win, wrapperEl, message);

    wrapperEl.appendChild(
      this._createElement(doc, "h2", {
        classList: "whatsNew-message-title",
        content: message.content.title,
      })
    );
    wrapperEl.appendChild(
      this._createElement(doc, "p", { content: message.content.body })
    );

    if (message.content.link_text) {
      wrapperEl.appendChild(
        this._createElement(doc, "a", {
          classList: "text-link",
          content: message.content.link_text,
        })
      );
    }

    return messageEl;
  }

  _createElement(doc, elem, options = {}) {
    const node = doc.createElementNS("http://www.w3.org/1999/xhtml", elem);
    if (options.classList) {
      node.classList.add(options.classList);
    }
    if (options.content) {
      this._setString(doc, node, options.content);
    }

    return node;
  }

  async _contentArguments() {
    // Between now and 6 weeks ago
    const dateTo = new Date();
    const dateFrom = new Date(dateTo.getTime() - 42 * 24 * 60 * 60 * 1000);
    const eventsByDate = await TrackingDBService.getEventsByDateRange(
      dateFrom,
      dateTo
    );
    // Count all events in the past 6 weeks
    const totalEvents = eventsByDate.reduce(
      (acc, day) => acc + day.getResultByName("count"),
      0
    );
    return {
      // Keys need to match variable names used in asrouter.ftl
      // `earliestDate` will be either 6 weeks ago or when tracking recording
      // started. Whichever is more recent.
      earliestDate: new Date(
        Math.max(
          new Date(await TrackingDBService.getEarliestRecordedDate()),
          dateFrom
        )
      ).getTime(),
      blockedCount: totalEvents.toLocaleString(),
    };
  }

  // If `string_id` is present it means we are relying on fluent for translations.
  // Otherwise, we have a vanilla string.
  _setString(doc, el, stringObj) {
    if (stringObj && stringObj.string_id) {
      doc.l10n.setAttributes(
        el,
        stringObj.string_id,
        // Pass all available arguments to Fluent
        this.state.contentArguments
      );
    } else {
      el.textContent = stringObj;
    }
  }

  // If `string_id` is present it means we are relying on fluent for translations.
  // Otherwise, we have a vanilla string.
  _setTextAttribute(doc, el, attr, stringObj) {
    if (stringObj && stringObj.string_id) {
      doc.l10n.setAttributes(el, stringObj.string_id);
    } else {
      el.setAttribute(attr, stringObj);
    }
  }

  _showAppmenuButton(win) {
    this.maybeInsertFTL(win);
    this._showElement(
      win.browser.ownerDocument,
      APPMENU_BUTTON_ID,
      BUTTON_STRING_ID
    );
  }

  _hideAppmenuButton(win) {
    this._hideElement(win.browser.ownerDocument, APPMENU_BUTTON_ID);
  }

  _showToolbarButton(win) {
    const document = win.browser.ownerDocument;
    this.maybeInsertFTL(win);
    this._showElement(document, TOOLBAR_BUTTON_ID, BUTTON_STRING_ID);
    // The toolbar dropdown panel uses this extra header element that is hidden
    // in the appmenu subview version of the panel. We only need to set it
    // when showing the toolbar button.
    document.l10n.setAttributes(
      document.querySelector(PANEL_HEADER_SELECTOR),
      "cfr-whatsnew-panel-header"
    );
  }

  _hideToolbarButton(win) {
    this._hideElement(win.browser.ownerDocument, TOOLBAR_BUTTON_ID);
  }

  _showElement(document, id, string_id) {
    const el = document.getElementById(id);
    document.l10n.setAttributes(el, string_id);
    el.removeAttribute("hidden");
  }

  _hideElement(document, id) {
    document.getElementById(id).setAttribute("hidden", true);
  }

  _sendTelemetry(ping) {
    this._dispatch({
      type: "TOOLBAR_PANEL_TELEMETRY",
      data: { action: "cfr_user_event", source: "CFR", ...ping },
    });
  }

  sendUserEventTelemetry(win, event, message, options = {}) {
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
        value: options.value,
      });
    }
  }

  /**
   * Inserts a message into the Protections Panel. The message is visible once
   * and afterwards set in a collapsed state. It can be shown again using the
   * info button in the panel header.
   */
  async insertProtectionPanelMessage(event) {
    const win = event.target.ownerGlobal;
    this.maybeInsertFTL(win);

    const doc = event.target.ownerDocument;
    const container = doc.getElementById("messaging-system-message-container");
    const infoButton = doc.getElementById("protections-popup-info-button");
    const panelContainer = doc.getElementById("protections-popup");
    const toggleMessage = () => {
      container.toggleAttribute("disabled");
      infoButton.toggleAttribute("checked");
      panelContainer.toggleAttribute("infoMessageShowing");
    };
    if (!container.childElementCount) {
      const message = await this._getMessages({
        template: "protections_panel",
        triggerId: "protectionsPanelOpen",
      });
      if (message) {
        const messageEl = this._createHeroElement(win, doc, message);
        container.appendChild(messageEl);
        infoButton.addEventListener("click", toggleMessage);
        this.sendUserEventTelemetry(win, "IMPRESSION", message.id);
      }
    }
    // Message is collapsed by default. If it was never shown before we want
    // to expand it
    if (
      !this.state.protectionPanelMessageSeen &&
      container.hasAttribute("disabled")
    ) {
      toggleMessage();
    }
    // Save state that we displayed the message
    if (!this.state.protectionPanelMessageSeen) {
      Services.prefs.setBoolPref(PROTECTIONS_PANEL_INFOMSG_PREF, true);
      this.state.protectionPanelMessageSeen = true;
    }
    // Collapse the message after the panel is hidden so we don't get the
    // animation when opening the panel
    panelContainer.addEventListener(
      "popuphidden",
      () => {
        if (
          this.state.protectionPanelMessageSeen &&
          !container.hasAttribute("disabled")
        ) {
          toggleMessage();
        }
      },
      {
        once: true,
      }
    );
  }

  /**
   * @param {object} browser MessageChannel target argument as a response to a user action
   * @param {object} message Message selected from devtools page
   */
  forceShowMessage(browser, message) {
    const win = browser.browser.ownerGlobal;
    const doc = browser.browser.ownerDocument;
    this.removeMessages(win, WHATS_NEW_PANEL_SELECTOR);
    this.renderMessages(win, doc, WHATS_NEW_PANEL_SELECTOR, {
      force: true,
      messages: [message],
    });
    win.PanelUI.panel.addEventListener("popuphidden", event =>
      this.removeMessages(event.target.ownerGlobal, WHATS_NEW_PANEL_SELECTOR)
    );
  }
}

this._ToolbarPanelHub = _ToolbarPanelHub;

/**
 * ToolbarPanelHub - singleton instance of _ToolbarPanelHub that can initiate
 * message requests and render messages.
 */
this.ToolbarPanelHub = new _ToolbarPanelHub();

const EXPORTED_SYMBOLS = ["ToolbarPanelHub", "_ToolbarPanelHub"];
