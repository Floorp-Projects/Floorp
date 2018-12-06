/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarView"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Receives and displays address bar autocomplete results.
 */
class UrlbarView {
  /**
   * @param {UrlbarInput} urlbar
   *   The UrlbarInput instance belonging to this UrlbarView instance.
   */
  constructor(urlbar) {
    this.urlbar = urlbar;
    this.panel = urlbar.panel;
    this.controller = urlbar.controller;
    this.document = urlbar.panel.ownerDocument;
    this.window = this.document.defaultView;

    this._mainContainer = this.panel.querySelector(".urlbarView-body-inner");
    this._rows = this.panel.querySelector(".urlbarView-results");

    // For the horizontal fade-out effect, set the overflow attribute on result
    // rows when they overflow.
    this._rows.addEventListener("overflow", this);
    this._rows.addEventListener("underflow", this);

    this.controller.addQueryListener(this);
  }

  get oneOffSearchButtons() {
    return this._oneOffSearchButtons ||
      (this._oneOffSearchButtons =
         new this.window.SearchOneOffs(this.panel.querySelector(".search-one-offs")));
  }

  /**
   * Opens the autocomplete results popup.
   */
  open() {
    this.panel.removeAttribute("hidden");

    this._alignPanel();

    // TODO: Search one off buttons are a stub right now.
    //       We'll need to set them up properly.
    this.oneOffSearchButtons;

    this.panel.openPopup(this.urlbar.textbox.closest("toolbar"), "after_end", 0, -1);

    this._rows.firstElementChild.toggleAttribute("selected", true);
  }

  /**
   * Closes the autocomplete results popup.
   */
  close() {
    this.panel.hidePopup();
  }

  // UrlbarController listener methods.
  onQueryStarted(queryContext) {
    this._rows.textContent = "";
  }

  onQueryCancelled(queryContext) {
    // Nothing.
  }

  onQueryFinished(queryContext) {
    // Nothing.
  }

  onQueryResults(queryContext) {
    // XXX For now, clear the results for each set received. We should really
    // be updating the existing list.
    this._rows.textContent = "";
    this._queryContext = queryContext;
    for (let resultIndex in queryContext.results) {
      this._addRow(resultIndex);
    }
    this.open();
  }

  // Private methods below.

  _getBoundsWithoutFlushing(element) {
    return this.window.windowUtils.getBoundsWithoutFlushing(element);
  }

  _createElement(name) {
    return this.document.createElementNS("http://www.w3.org/1999/xhtml", name);
  }

  _alignPanel() {
    // Make the panel span the width of the window.
    let documentRect =
      this._getBoundsWithoutFlushing(this.document.documentElement);
    let width = documentRect.right - documentRect.left;
    this.panel.setAttribute("width", width);

    // Subtract two pixels for left and right borders on the panel.
    this._mainContainer.style.maxWidth = (width - 2) + "px";

    // Keep the popup items' site icons aligned with the urlbar's identity
    // icon if it's not too far from the edge of the window.  We define
    // "too far" as "more than 30% of the window's width AND more than
    // 250px".
    let boundToCheck = this.window.RTL_UI ? "right" : "left";
    let inputRect = this._getBoundsWithoutFlushing(this.urlbar.textbox);
    let startOffset = Math.abs(inputRect[boundToCheck] - documentRect[boundToCheck]);
    let alignSiteIcons = startOffset / width <= 0.3 || startOffset <= 250;
    if (alignSiteIcons) {
      // Calculate the end margin if we have a start margin.
      let boundToCheckEnd = this.window.RTL_UI ? "left" : "right";
      let endOffset = Math.abs(inputRect[boundToCheckEnd] -
                               documentRect[boundToCheckEnd]);
      if (endOffset > startOffset * 2) {
        // Provide more space when aligning would result in an unbalanced
        // margin. This allows the location bar to be moved to the start
        // of the navigation toolbar to reclaim space for results.
        endOffset = startOffset;
      }
      let identityIcon = this.document.getElementById("identity-icon");
      let identityRect = this._getBoundsWithoutFlushing(identityIcon);
      let start = this.window.RTL_UI ?
                    documentRect.right - identityRect.right :
                    identityRect.left;

      this.panel.style.setProperty("--item-padding-start", Math.round(start) + "px");
      this.panel.style.setProperty("--item-padding-end", Math.round(endOffset) + "px");
    } else {
      this.panel.style.removeProperty("--item-padding-start");
      this.panel.style.removeProperty("--item-padding-end");
    }
  }

  _addRow(resultIndex) {
    let result = this._queryContext.results[resultIndex];
    let item = this._createElement("div");
    item.className = "urlbarView-row";
    item.addEventListener("click", this);
    item.setAttribute("resultIndex", resultIndex);
    if (result.type == UrlbarUtils.MATCH_TYPE.TAB_SWITCH) {
      item.setAttribute("action", "switch-to-tab");
    }

    let content = this._createElement("span");
    content.className = "urlbarView-row-inner";
    item.appendChild(content);

    let actionIcon = this._createElement("span");
    actionIcon.className = "urlbarView-action-icon";
    content.appendChild(actionIcon);

    let favicon = this._createElement("img");
    favicon.className = "urlbarView-favicon";
    favicon.src = result.payload.icon || "chrome://mozapps/skin/places/defaultFavicon.svg";
    content.appendChild(favicon);

    let title = this._createElement("span");
    title.className = "urlbarView-title";
    title.textContent = result.title || result.payload.url;
    content.appendChild(title);

    let secondary = this._createElement("span");
    secondary.className = "urlbarView-secondary";
    if (result.type == UrlbarUtils.MATCH_TYPE.TAB_SWITCH) {
      secondary.classList.add("urlbarView-action");
      secondary.textContent = "Switch to Tab";
    } else {
      secondary.classList.add("urlbarView-url");
      secondary.textContent = result.payload.url;
    }
    content.appendChild(secondary);

    this._rows.appendChild(item);
  }

  /**
   * Passes DOM events for the view to the _on_<event type> methods.
   * @param {Event} event
   *   DOM event from the <view>.
   */
  handleEvent(event) {
    let methodName = "_on_" + event.type;
    if (methodName in this) {
      this[methodName](event);
    } else {
      throw new Error("Unrecognized UrlbarView event: " + event.type);
    }
  }

  _on_click(event) {
    let row = event.target;
    while (!row.classList.contains("urlbarView-row")) {
      row = row.parentNode;
    }
    let resultIndex = row.getAttribute("resultIndex");
    let result = this._queryContext.results[resultIndex];
    if (result) {
      this.urlbar.resultSelected(event, result);
    }
    this.close();
  }

  _on_overflow(event) {
    if (event.target.classList.contains("urlbarView-row-inner")) {
      event.target.toggleAttribute("overflow", true);
    }
  }

  _on_underflow(event) {
    if (event.target.classList.contains("urlbarView-row-inner")) {
      event.target.toggleAttribute("overflow", false);
    }
  }
}
