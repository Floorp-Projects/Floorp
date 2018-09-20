/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarInput"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  QueryContext: "resource:///modules/UrlbarController.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarView: "resource:///modules/UrlbarView.jsm",
});

/**
 * Represents the urlbar <textbox>.
 * Also forwards important textbox properties and methods.
 */
class UrlbarInput {
  /**
   * @param {object} options
   *   The initial options for UrlbarInput.
   * @param {object} options.textbox
   *   The <textbox> element.
   * @param {object} options.panel
   *   The <panel> element.
   * @param {UrlbarController} [options.controller]
   *   Optional fake controller to override the built-in UrlbarController.
   *   Intended for use in unit tests only.
   */
  constructor(options = {}) {
    this.textbox = options.textbox;
    this.panel = options.panel;
    this.controller = options.controller || new UrlbarController();
    this.view = new UrlbarView(this);
    this.valueIsTyped = false;
    this.userInitiatedFocus = false;
    this.isPrivate = PrivateBrowsingUtils.isWindowPrivate(this.panel.ownerGlobal);

    const METHODS = ["addEventListener", "removeEventListener",
      "setAttribute", "hasAttribute", "removeAttribute", "getAttribute",
      "focus", "blur", "select"];
    const READ_ONLY_PROPERTIES = ["focused", "inputField", "editor"];
    const READ_WRITE_PROPERTIES = ["value", "placeholder", "readOnly",
      "selectionStart", "selectionEnd"];

    for (let method of METHODS) {
      this[method] = (...args) => {
        this.textbox[method](...args);
      };
    }

    for (let property of READ_ONLY_PROPERTIES) {
      Object.defineProperty(this, property, {
        enumerable: true,
        get() {
          return this.textbox[property];
        },
      });
    }

    for (let property of READ_WRITE_PROPERTIES) {
      Object.defineProperty(this, property, {
        enumerable: true,
        get() {
          return this.textbox[property];
        },
        set(val) {
          return this.textbox[property] = val;
        },
      });
    }

    this.addEventListener("input", this);
  }

  formatValue() {
  }

  closePopup() {
    this.view.close();
  }

  openResults() {
    this.view.open();
  }

  /**
   * Passes DOM events for the textbox to the _on<event type> methods.
   * @param {Event} event
   *   DOM event from the <textbox>.
   */
  handleEvent(event) {
    let methodName = "_on" + event.type;
    if (methodName in this) {
      this[methodName](event);
    } else {
      throw "Unrecognized urlbar event: " + event.type;
    }
  }

  // Private methods below.

  _oninput(event) {
    // XXX Fill in lastKey & maxResults, and add anything else we need.
    this.controller.handleQuery(new QueryContext({
      searchString: event.target.value,
      lastKey: "",
      maxResults: 12,
      isPrivate: this.isPrivate,
    }));
  }
}
