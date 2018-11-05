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
    this._rows.addEventListener("overflow", event => {
      if (event.target.classList.contains("urlbarView-row-inner")) {
        event.target.toggleAttribute("overflow", true);
      }
    });
    this._rows.addEventListener("underflow", event => {
      if (event.target.classList.contains("urlbarView-row-inner")) {
        event.target.toggleAttribute("overflow", false);
      }
    });

    this.controller.addQueryListener(this);
  }

  /**
   * Opens the autocomplete results popup.
   */
  open() {
    this.panel.removeAttribute("hidden");

    let panelDirection = this.panel.style.direction;
    if (!panelDirection) {
      panelDirection = this.panel.style.direction =
        this.window.getComputedStyle(this.urlbar.textbox).direction;
    }

    // Make the panel span the width of the window.
    let documentRect =
      this._getBoundsWithoutFlushing(this.document.documentElement);
    let width = documentRect.right - documentRect.left;
    this.panel.setAttribute("width", width);

    // Subtract two pixels for left and right borders on the panel.
    this._mainContainer.style.maxWidth = (width - 2) + "px";

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

    let favicon = this._createElement("span");
    favicon.className = "urlbarView-favicon";
    content.appendChild(favicon);

    let title = this._createElement("span");
    title.className = "urlbarView-title";
    title.textContent = result.title || result.url;
    content.appendChild(title);

    let secondary = this._createElement("span");
    secondary.className = "urlbarView-secondary";
    if (result.type == UrlbarUtils.MATCH_TYPE.TAB_SWITCH) {
      secondary.classList.add("urlbarView-action");
      secondary.textContent = "Switch to Tab";
    } else {
      secondary.classList.add("urlbarView-url");
      secondary.textContent = result.url;
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
      throw "Unrecognized urlbar event: " + event.type;
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
}
