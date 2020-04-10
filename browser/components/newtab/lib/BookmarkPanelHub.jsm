/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm"
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

class _BookmarkPanelHub {
  constructor() {
    this._id = "BookmarkPanelHub";
    this._trigger = { id: "bookmark-panel" };
    this._handleMessageRequest = null;
    this._addImpression = null;
    this._dispatch = null;
    this._initialized = false;
    this._response = null;
    this._l10n = null;

    this.messageRequest = this.messageRequest.bind(this);
    this.toggleRecommendation = this.toggleRecommendation.bind(this);
    this.sendUserEventTelemetry = this.sendUserEventTelemetry.bind(this);
    this.collapseMessage = this.collapseMessage.bind(this);
  }

  /**
   * @param {function} handleMessageRequest
   * @param {function} addImpression
   * @param {function} dispatch - Used for sending user telemetry information
   */
  init(handleMessageRequest, addImpression, dispatch) {
    this._handleMessageRequest = handleMessageRequest;
    this._addImpression = addImpression;
    this._dispatch = dispatch;
    this._l10n = new DOMLocalization([]);
    this._initialized = true;
  }

  uninit() {
    this._l10n = null;
    this._initialized = false;
    this._handleMessageRequest = null;
    this._addImpression = null;
    this._dispatch = null;
    this._response = null;
  }

  /**
   * Checks if a similar cached requests exists before forwarding the request
   * to ASRouter. Caches only 1 request, unique identifier is `request.url`.
   * Caching ensures we don't duplicate requests and telemetry pings.
   * Return value is important for the caller to know if a message will be
   * shown.
   *
   * @returns {obj|null} response object or null if no messages matched
   */
  async messageRequest(target, win) {
    if (!this._initialized) {
      return false;
    }

    if (
      this._response &&
      this._response.win === win &&
      this._response.url === target.url &&
      this._response.content
    ) {
      this.showMessage(this._response.content, target, win);
      return true;
    }

    // If we didn't match on a previously cached request then make sure
    // the container is empty
    this._removeContainer(target);
    const response = await this._handleMessageRequest({
      triggerId: this._trigger.id,
    });

    return this.onResponse(response, target, win);
  }

  /**
   * If the response contains a message render it and send an impression.
   * Otherwise we remove the message from the container.
   */
  onResponse(response, target, win) {
    this._response = {
      ...response,
      collapsed: false,
      target,
      win,
      url: target.url,
    };

    if (response && response.content) {
      // Only insert localization files if we need to show a message
      win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
      win.MozXULElement.insertFTLIfNeeded("browser/branding/sync-brand.ftl");
      this.showMessage(response.content, target, win);
      this.sendImpression();
      this.sendUserEventTelemetry("IMPRESSION", win);
    } else {
      this.hideMessage(target);
    }

    target.infoButton.disabled = !response;

    return !!response;
  }

  showMessage(message, target, win) {
    if (this._response && this._response.collapsed) {
      this.toggleRecommendation(false);
      return;
    }

    const createElement = elem =>
      target.document.createElementNS("http://www.w3.org/1999/xhtml", elem);
    let recommendation = target.container.querySelector("#cfrMessageContainer");
    if (!recommendation) {
      recommendation = createElement("div");
      const headerContainer = createElement("div");
      headerContainer.classList.add("cfrMessageHeader");
      recommendation.setAttribute("id", "cfrMessageContainer");
      recommendation.addEventListener("click", async e => {
        target.hidePopup();
        const url = await FxAccounts.config.promiseConnectAccountURI(
          "bookmark"
        );
        win.ownerGlobal.openLinkIn(url, "tabshifted", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
            {}
          ),
          csp: null,
        });
        this.sendUserEventTelemetry("CLICK", win);
      });
      recommendation.style.color = message.color;
      recommendation.style.background = `-moz-linear-gradient(-45deg, ${message.background_color_1} 0%, ${message.background_color_2} 70%)`;
      const close = createElement("button");
      close.setAttribute("id", "cfrClose");
      close.setAttribute("aria-label", "close");
      close.style.color = message.color;
      close.addEventListener("click", e => {
        this.sendUserEventTelemetry("DISMISS", win);
        this.collapseMessage();
        target.close(e);
      });
      const title = createElement("h1");
      title.setAttribute("id", "editBookmarkPanelRecommendationTitle");
      const content = createElement("p");
      content.setAttribute("id", "editBookmarkPanelRecommendationContent");
      const cta = createElement("button");
      cta.setAttribute("id", "editBookmarkPanelRecommendationCta");

      // If `string_id` is present it means we are relying on fluent for translations
      if (message.text.string_id) {
        this._l10n.setAttributes(
          close,
          message.close_button.tooltiptext.string_id
        );
        this._l10n.setAttributes(title, message.title.string_id);
        this._l10n.setAttributes(content, message.text.string_id);
        this._l10n.setAttributes(cta, message.cta.string_id);
      } else {
        close.setAttribute("title", message.close_button.tooltiptext);
        title.textContent = message.title;
        content.textContent = message.text;
        cta.textContent = message.cta;
      }

      headerContainer.appendChild(title);
      headerContainer.appendChild(close);
      recommendation.appendChild(headerContainer);
      recommendation.appendChild(content);
      recommendation.appendChild(cta);
      target.container.appendChild(recommendation);
    }

    this.toggleRecommendation(true);
    this._adjustPanelHeight(win, recommendation);
  }

  /**
   * Adjust the size of the container for locales where the message is
   * longer than the fixed 150px set for height
   */
  async _adjustPanelHeight(window, messageContainer) {
    const { document } = window;
    // Contains the screenshot of the page we are bookmarking
    const screenshotContainer = document.getElementById(
      "editBookmarkPanelImage"
    );
    // Wait for strings to be added which can change element height
    await document.l10n.translateElements([messageContainer]);
    window.requestAnimationFrame(() => {
      let { height } = messageContainer.getBoundingClientRect();
      if (height > 150) {
        messageContainer.classList.add("longMessagePadding");
        // Get the new value with the added padding
        height = messageContainer.getBoundingClientRect().height;
        // Needs to be adjusted to match the message height
        screenshotContainer.style.height = `${height}px`;
      }
    });
  }

  /**
   * Restore the panel back to the original size so the slide in
   * animation can run again
   */
  _restorePanelHeight(window) {
    const { document } = window;
    // Contains the screenshot of the page we are bookmarking
    document.getElementById("editBookmarkPanelImage").style.height = "";
  }

  toggleRecommendation(visible) {
    if (!this._response) {
      return;
    }

    const { target } = this._response;
    if (visible === undefined) {
      // When called from the info button of the bookmark panel
      target.infoButton.checked = !target.infoButton.checked;
    } else {
      target.infoButton.checked = visible;
    }
    if (target.infoButton.checked) {
      // If it was ever collapsed we need to cancel the state
      this._response.collapsed = false;
      target.container.removeAttribute("disabled");
    } else {
      target.container.setAttribute("disabled", "disabled");
    }
  }

  collapseMessage() {
    this._response.collapsed = true;
    this.toggleRecommendation(false);
  }

  _removeContainer(target) {
    if (target || (this._response && this._response.target)) {
      const container = (
        target || this._response.target
      ).container.querySelector("#cfrMessageContainer");
      if (container) {
        this._restorePanelHeight(this._response.win);
        container.remove();
      }
    }
  }

  hideMessage(target) {
    this._removeContainer(target);
    this.toggleRecommendation(false);
    this._response = null;
  }

  _forceShowMessage(target, message) {
    const doc = target.browser.ownerGlobal.gBrowser.ownerDocument;
    const win = target.browser.ownerGlobal.window;
    const panelTarget = {
      container: doc.getElementById("editBookmarkPanelRecommendation"),
      infoButton: doc.getElementById("editBookmarkPanelInfoButton"),
      document: doc,
      close: e => {
        e.stopPropagation();
        this.toggleRecommendation(false);
      },
    };
    // Remove any existing message
    this.hideMessage(panelTarget);
    // Reset the reference to the panel elements
    this._response = { target: panelTarget, win };
    // Required if we want to preview messages that include fluent strings
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
    win.MozXULElement.insertFTLIfNeeded("browser/branding/sync-brand.ftl");
    this.showMessage(message.content, panelTarget, win);
  }

  sendImpression() {
    this._addImpression(this._response);
  }

  sendUserEventTelemetry(event, win) {
    // Only send pings for non private browsing windows
    if (
      !PrivateBrowsingUtils.isBrowserPrivate(
        win.ownerGlobal.gBrowser.selectedBrowser
      )
    ) {
      this._sendTelemetry({
        message_id: this._response.id,
        bucket_id: this._response.id,
        event,
      });
    }
  }

  _sendTelemetry(ping) {
    this._dispatch({
      type: "DOORHANGER_TELEMETRY",
      data: { action: "cfr_user_event", source: "CFR", ...ping },
    });
  }
}

this._BookmarkPanelHub = _BookmarkPanelHub;

/**
 * BookmarkPanelHub - singleton instance of _BookmarkPanelHub that can initiate
 * message requests and render messages.
 */
this.BookmarkPanelHub = new _BookmarkPanelHub();

const EXPORTED_SYMBOLS = ["BookmarkPanelHub", "_BookmarkPanelHub"];
