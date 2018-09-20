/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarView"];

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

    this._rows.textContent = "";
    for (let i = 0; i < 12; i++) {
      this._addRow();
    }
    this._rows.firstElementChild.toggleAttribute("selected", true);
  }

  /**
   * Closes the autocomplete results popup.
   */
  close() {
  }

  // Private methods below.
  /* eslint-disable require-jsdoc */

  _getBoundsWithoutFlushing(element) {
    return this.window.windowUtils.getBoundsWithoutFlushing(element);
  }

  _createElement(name) {
    return this.document.createElementNS("http://www.w3.org/1999/xhtml", name);
  }

  _addRow() {
    const SWITCH_TO_TAB = Math.random() < .3;

    let item = this._createElement("div");
    item.className = "urlbarView-row";
    if (SWITCH_TO_TAB) {
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
    do {
      title.textContent += "foo bar ";
    } while (Math.random() < .5);
    content.appendChild(title);

    let secondary = this._createElement("span");
    secondary.className = "urlbarView-secondary";
    if (SWITCH_TO_TAB) {
      secondary.classList.add("urlbarView-action");
      secondary.textContent = "Switch to Tab";
    } else {
      secondary.classList.add("urlbarView-url");
      secondary.textContent = "http://www";
      while (Math.random() < .95) {
        secondary.textContent += ".xyz";
      }
    }
    content.appendChild(secondary);

    this._rows.appendChild(item);
  }
}
