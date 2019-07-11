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

const WHATSNEW_ENABLED_PREF = "browser.messaging-system.whatsNewPanel.enabled";

const TOOLBAR_BUTTON_ID = "whats-new-menu-button";
const APPMENU_BUTTON_ID = "appMenu-whatsnew-button";
const PANEL_HEADER_SELECTOR = "#PanelUI-whatsNew-title > label";

const BUTTON_STRING_ID = "cfr-whatsnew-button";

class _ToolbarPanelHub {
  constructor() {
    this._showAppmenuButton = this._showAppmenuButton.bind(this);
    this._hideAppmenuButton = this._hideAppmenuButton.bind(this);
    this._showToolbarButton = this._showToolbarButton.bind(this);
    this._hideToolbarButton = this._hideToolbarButton.bind(this);
  }

  init({ getMessages }) {
    this._getMessages = getMessages;
    if (this.whatsNewPanelEnabled) {
      this.enableAppmenuButton();
    }
  }

  uninit() {
    EveryWindow.unregisterCallback(TOOLBAR_BUTTON_ID);
    EveryWindow.unregisterCallback(APPMENU_BUTTON_ID);
  }

  get whatsNewPanelEnabled() {
    return Services.prefs.getBoolPref(WHATSNEW_ENABLED_PREF, false);
  }

  maybeInsertFTL(win) {
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
  }

  // Turns on the Appmenu (hamburger menu) button for all open windows and future windows.
  enableAppmenuButton() {
    EveryWindow.registerCallback(
      APPMENU_BUTTON_ID,
      this._showAppmenuButton,
      this._hideAppmenuButton
    );
  }

  // Turns on the Toolbar button for all open windows and future windows.
  enableToolbarButton() {
    EveryWindow.registerCallback(
      TOOLBAR_BUTTON_ID,
      this._showToolbarButton,
      this._hideToolbarButton
    );
  }

  // Render what's new messages into the panel.
  async renderMessages(win, doc, containerId) {
    const messages = (await this._getMessages({
      template: "whatsnew_panel_message",
      triggerId: "whatsNewPanelOpened",
      returnAll: true,
    })).sort((m1, m2) => {
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
      for (let { content } of messages) {
        container.appendChild(
          this._createMessageElements(win, doc, content, previousDate)
        );
        previousDate = content.published_date;
      }
    }

    // TODO: TELEMETRY
  }

  _createMessageElements(win, doc, content, previousDate) {
    const messageEl = this._createElement(doc, "div");
    messageEl.classList.add("whatsNew-message");

    // Only render date if it is different from the one rendered before.
    if (content.published_date !== previousDate) {
      messageEl.appendChild(
        this._createDateElement(doc, content.published_date)
      );
    }

    const wrapperEl = this._createElement(doc, "div");
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

      // TODO: TELEMETRY
    });

    if (content.icon_url) {
      wrapperEl.classList.add("has-icon");
      const iconEl = this._createElement(doc, "img");
      iconEl.src = content.icon_url;
      iconEl.classList.add("whatsNew-message-icon");
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
      const linkEl = this._createElement(doc, "button");
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
}

this._ToolbarPanelHub = _ToolbarPanelHub;

/**
 * ToolbarPanelHub - singleton instance of _ToolbarPanelHub that can initiate
 * message requests and render messages.
 */
this.ToolbarPanelHub = new _ToolbarPanelHub();

const EXPORTED_SYMBOLS = ["ToolbarPanelHub", "_ToolbarPanelHub"];
