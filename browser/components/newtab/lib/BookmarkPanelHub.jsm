/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(this, "DOMLocalization",
  "resource://gre/modules/DOMLocalization.jsm");

class _BookmarkPanelHub {
  constructor() {
    this._id = "BookmarkPanelHub";
    this._dispatchToASR = null;
    this._initalized = false;
    this._response = null;
    this._l10n = null;

    this.dispatch = this.dispatch.bind(this);
    this.messageRequest = this.messageRequest.bind(this);
  }

  init(dispatch) {
    this._dispatchToASR = dispatch;
    this._l10n = new DOMLocalization([
      "browser/newtab/asrouter.ftl",
    ]);
    this._initalized = true;
  }

  uninit() {
    this._l10n = null;
    this._dispatchToASR = null;
    this._initalized = false;
  }

  dispatch(message, target) {
    return this._dispatchToASR(message, target);
  }

  /**
   * Checks if a similar cached requests exists before forwarding the request
   * to ASRouter. Caches only 1 request, unique identifier is `request.url`.
   * Caching ensures we don't duplicate requests and telemetry pings.
   */
  async messageRequest(target, win) {
    if (this._request && this._message.url === target.url) {
      this.onResponse(this._response, target);
      return !!this._response;
    }

    const waitForASRMessage = new Promise((resolve, reject) => {
      this.dispatch({
        type: "MESSAGE_REQUEST",
        source: this._id,
        data: {trigger: {id: "bookmark-panel"}},
      }, {sendMessage: resolve});
    });

    const response = await waitForASRMessage;
    this.onResponse(response, target, win);
    return !!this._response;
  }

  /**
   * If the response contains a message render it and send an impression.
   * Otherwise we remove the message from the container.
   */
  onResponse(response, target, win) {
    if (response && response.message) {
      this._response = {
        ...response,
        url: target.url,
      };

      this.showMessage(response.message.content, target, win);
      this.sendImpression();
    } else {
      // If we didn't get a message response we need to remove and clear
      // any existing messages
      this._response = null;
      this.hideMessage(target);
    }
  }

  showMessage(message, target, win) {
    const {createElement} = target;
    if (!target.container.querySelector("#cfrMessageContainer")) {
      const recommendation = createElement("div");
      recommendation.setAttribute("id", "cfrMessageContainer");
      recommendation.addEventListener("click", e => {
        target.hidePopup();
        this._dispatchToASR(
          {type: "USER_ACTION", data: {
            type: "SHOW_FIREFOX_ACCOUNTS",
            entrypoint: "bookmark",
            method: "emailFirst",
            where: "tabshifted"
          }},
          {browser: win.gBrowser.selectedBrowser});
      });
      recommendation.style.color = message.color;
      recommendation.style.background = `-moz-linear-gradient(-45deg, ${message.background_color_1} 0%, ${message.background_color_2} 70%)`;
      const close = createElement("a");
      close.setAttribute("id", "cfrClose");
      close.setAttribute("aria-label", "close"); // TODO localize
      close.addEventListener("click", target.close);
      const title = createElement("h1");
      title.setAttribute("id", "editBookmarkPanelRecommendationTitle");
      title.textContent = "Sync your bookmarks everywhere.";
      this._l10n.setAttributes(title, message.title);
      const content = createElement("p");
      content.setAttribute("id", "editBookmarkPanelRecommendationContent");
      content.textContent = "Great find! Now don’t be left without this bookmark on your mobile devices. Get started with a Firefox Account.";
      this._l10n.setAttributes(content, message.text);
      const cta = createElement("button");
      cta.setAttribute("id", "editBookmarkPanelRecommendationCta");
      cta.textContent = "Sync my bookmarks …";
      this._l10n.setAttributes(cta, message.cta);
      recommendation.appendChild(close);
      recommendation.appendChild(title);
      recommendation.appendChild(content);
      recommendation.appendChild(cta);
      // this._l10n.translateElements([...recommendation.children]);
      target.container.appendChild(recommendation);
    }
  }

  hideMessage(target) {
    const container = target.container.querySelector("#cfrMessageContainer");
    if (container) {
      container.remove();
    }
  }

  sendImpression() {
    this.dispatch({
      type: "IMPRESSION",
      source: this._id,
      data: {},
    });
  }
}

this.BookmarkPanelHub = new _BookmarkPanelHub();

const EXPORTED_SYMBOLS = ["BookmarkPanelHub"];
