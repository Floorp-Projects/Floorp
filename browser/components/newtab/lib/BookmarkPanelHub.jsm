/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(this, "DOMLocalization",
  "resource://gre/modules/DOMLocalization.jsm");
ChromeUtils.defineModuleGetter(this, "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

class _BookmarkPanelHub {
  constructor() {
    this._id = "BookmarkPanelHub";
    this._trigger = {id: "bookmark-panel"};
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
    this._l10n = new DOMLocalization([
      "browser/branding/sync-brand.ftl",
      "browser/newtab/asrouter.ftl",
    ]);
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

    if (this._response && this._response.win === win && this._response.url === target.url && this._response.content) {
      this.showMessage(this._response.content, target, win);
      return true;
    }

    const response = await this._handleMessageRequest(this._trigger);

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
    if (this._response.collapsed) {
      this.toggleRecommendation(false);
      return;
    }

    const createElement = elem => target.document.createElementNS("http://www.w3.org/1999/xhtml", elem);

    if (!target.container.querySelector("#cfrMessageContainer")) {
      const recommendation = createElement("div");
      recommendation.setAttribute("id", "cfrMessageContainer");
      recommendation.addEventListener("click", async e => {
        target.hidePopup();
        const url = await FxAccounts.config.promiseEmailFirstURI("bookmark");
        win.ownerGlobal.openLinkIn(url, "tabshifted", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
          csp: null,
        });
        this.sendUserEventTelemetry("CLICK", win);
      });
      recommendation.style.color = message.color;
      recommendation.style.background = `-moz-linear-gradient(-45deg, ${message.background_color_1} 0%, ${message.background_color_2} 70%)`;
      const close = createElement("a");
      close.setAttribute("id", "cfrClose");
      close.setAttribute("aria-label", "close");
      this._l10n.setAttributes(close, message.close_button.tooltiptext);
      close.addEventListener("click", e => {
        this.sendUserEventTelemetry("DISMISS", win);
        this.collapseMessage();
        target.close(e);
      });
      const title = createElement("h1");
      title.setAttribute("id", "editBookmarkPanelRecommendationTitle");
      this._l10n.setAttributes(title, message.title);
      const content = createElement("p");
      content.setAttribute("id", "editBookmarkPanelRecommendationContent");
      this._l10n.setAttributes(content, message.text);
      const cta = createElement("button");
      cta.setAttribute("id", "editBookmarkPanelRecommendationCta");
      this._l10n.setAttributes(cta, message.cta);
      recommendation.appendChild(close);
      recommendation.appendChild(title);
      recommendation.appendChild(content);
      recommendation.appendChild(cta);
      this._l10n.translateElements([...recommendation.children]);
      target.container.appendChild(recommendation);
    }

    this.toggleRecommendation(true);
  }

  toggleRecommendation(visible) {
    const {target} = this._response;
    if (visible === undefined) {
      // When called from the info button of the bookmark panel
      target.infoButton.checked = !target.infoButton.checked;
    } else {
      target.infoButton.checked = visible;
    }
    if (target.infoButton.checked) {
      // If it was ever collapsed we need to cancel the state
      this._response.collapsed = false;
      target.recommendationContainer.removeAttribute("disabled");
    } else {
      target.recommendationContainer.setAttribute("disabled", "disabled");
    }
  }

  collapseMessage() {
    this._response.collapsed = true;
    this.toggleRecommendation(false);
  }

  hideMessage(target) {
    const container = target.container.querySelector("#cfrMessageContainer");
    if (container) {
      container.remove();
    }
    this.toggleRecommendation(false);
    this._response = null;
  }

  _forceShowMessage(message) {
    this.toggleRecommendation(true);
    this.showMessage(message.content, this._response.target, this._response.win);
  }

  sendImpression() {
    this._addImpression(this._response);
  }

  sendUserEventTelemetry(event, win) {
    // Only send pings for non private browsing windows
    if (!PrivateBrowsingUtils.isBrowserPrivate(win.ownerGlobal.gBrowser.selectedBrowser)) {
      this._sendTelemetry({message_id: this._response.id, bucket_id: this._response.id, event});
    }
  }

  _sendTelemetry(ping) {
    this._dispatch({
      type: "DOORHANGER_TELEMETRY",
      data: {action: "cfr_user_event", source: "CFR", ...ping},
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
