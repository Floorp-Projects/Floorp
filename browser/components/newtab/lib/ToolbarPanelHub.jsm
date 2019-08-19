/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "EveryWindow",
  "resource:///modules/EveryWindow.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

const WHATSNEW_ENABLED_PREF = "browser.messaging-system.whatsNewPanel.enabled";
const PROTECTIONS_PANEL_INFOMSG_PREF =
  "browser.protections_panel.infoMessage.seen";

const TOOLBAR_BUTTON_ID = "whats-new-menu-button";
const APPMENU_BUTTON_ID = "appMenu-whatsnew-button";
const PANEL_HEADER_SELECTOR = "#PanelUI-whatsNew-title > label";

const BUTTON_STRING_ID = "cfr-whatsnew-button";

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

  async init(waitForInitialized, { getMessages, dispatch }) {
    this._getMessages = getMessages;
    this._dispatch = dispatch;
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

  // Render what's new messages into the panel.
  async renderMessages(win, doc, containerId) {
    const messages = (await this.messages).sort((m1, m2) => {
      // Sort by published_date in descending order.
      if (m1.content.published_date === m2.content.published_date) {
        return 0;
      }
      if (m1.content.published_date > m2.content.published_date) {
        return -1;
      }
      return 1;
    });
    const container = doc.getElementById(containerId);

    if (messages && !container.querySelector(".whatsNew-message")) {
      let previousDate = 0;
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

  _createMessageElements(win, doc, message, previousDate) {
    const { content } = message;
    const messageEl = this._createElement(doc, "div");
    messageEl.classList.add("whatsNew-message");

    // Only render date if it is different from the one rendered before.
    if (content.published_date !== previousDate) {
      messageEl.appendChild(
        this._createDateElement(doc, content.published_date)
      );
    }

    const wrapperEl = this._createElement(doc, "button");
    wrapperEl.doCommand = () => {};
    wrapperEl.classList.add("whatsNew-message-body");
    messageEl.appendChild(wrapperEl);
    wrapperEl.addEventListener("click", () => {
      win.ownerGlobal.openLinkIn(content.cta_url, "tabshifted", {
        private: false,
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
        csp: null,
      });

      this.sendUserEventTelemetry(win, "CLICK", message);
    });

    if (content.icon_url) {
      wrapperEl.classList.add("has-icon");
      const iconEl = this._createElement(doc, "img");
      iconEl.src = content.icon_url;
      iconEl.classList.add("whatsNew-message-icon");
      this._setTextAttribute(doc, iconEl, "alt", content.icon_alt);
      wrapperEl.appendChild(iconEl);
    }

    const titleEl = this._createElement(doc, "h2");
    titleEl.classList.add("whatsNew-message-title");
    this._setString(doc, titleEl, content.title);
    wrapperEl.appendChild(titleEl);

    const bodyEl = this._createElement(doc, "p");
    this._setString(doc, bodyEl, content.body);
    wrapperEl.appendChild(bodyEl);

    if (content.link_text) {
      const linkEl = this._createElement(doc, "a");
      linkEl.classList.add("text-link");
      this._setString(doc, linkEl, content.link_text);
      wrapperEl.appendChild(linkEl);
    }

    return messageEl;
  }

  _createHeroElement(win, doc, content) {
    const messageEl = this._createElement(doc, "div");
    messageEl.setAttribute("id", "protections-popup-message");
    messageEl.classList.add("whatsNew-hero-message");
    const wrapperEl = this._createElement(doc, "div");
    wrapperEl.classList.add("whatsNew-message-body");
    messageEl.appendChild(wrapperEl);
    wrapperEl.addEventListener("click", () => {
      win.ownerGlobal.openLinkIn(content.cta_url, "tabshifted", {
        private: false,
        relatedToCurrent: true,
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
        csp: null,
      });
    });
    const titleEl = this._createElement(doc, "h2");
    titleEl.classList.add("whatsNew-message-title");
    this._setString(doc, titleEl, content.title);
    wrapperEl.appendChild(titleEl);

    const bodyEl = this._createElement(doc, "p");
    this._setString(doc, bodyEl, content.body);
    wrapperEl.appendChild(bodyEl);

    if (content.link_text) {
      const linkEl = this._createElement(doc, "a");
      linkEl.classList.add("text-link");
      this._setString(doc, linkEl, content.link_text);
      wrapperEl.appendChild(linkEl);
    }

    return messageEl;
  }

  _createElement(doc, elem) {
    return doc.createElementNS("http://www.w3.org/1999/xhtml", elem);
  }

  _createDateElement(doc, date) {
    const dateEl = this._createElement(doc, "p");
    dateEl.classList.add("whatsNew-message-date");
    dateEl.textContent = new Date(date).toLocaleDateString("default", {
      month: "long",
      day: "numeric",
      year: "numeric",
    });
    return dateEl;
  }

  // If `string_id` is present it means we are relying on fluent for translations.
  // Otherwise, we have a vanilla string.
  _setString(doc, el, stringObj) {
    if (stringObj.string_id) {
      doc.l10n.setAttributes(el, stringObj.string_id);
    } else {
      el.textContent = stringObj;
    }
  }

  // If `string_id` is present it means we are relying on fluent for translations.
  // Otherwise, we have a vanilla string.
  _setTextAttribute(doc, el, attr, stringObj) {
    if (stringObj.string_id) {
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
    };
    if (!container.childElementCount) {
      const message = await this._getMessages({
        template: "protections_panel",
        triggerId: "protectionsPanelOpen",
      });
      if (message) {
        const messageEl = this._createHeroElement(win, doc, message.content);
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
}

this._ToolbarPanelHub = _ToolbarPanelHub;

/**
 * ToolbarPanelHub - singleton instance of _ToolbarPanelHub that can initiate
 * message requests and render messages.
 */
this.ToolbarPanelHub = new _ToolbarPanelHub();

const EXPORTED_SYMBOLS = ["ToolbarPanelHub", "_ToolbarPanelHub"];
