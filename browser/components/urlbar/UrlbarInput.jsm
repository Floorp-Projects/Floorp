/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarInput"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarView: "resource:///modules/UrlbarView.jsm",
});

/**
 * Represents the urlbar <textbox>.
 * Also forwards important textbox properties and methods.
 */
class UrlbarInput {
  /**
   * @param {object} textbox
   *   The <textbox> element.
   * @param {object} panel
   *   The <panel> element.
   */
  constructor(textbox, panel) {
    this.textbox = textbox;
    this.panel = panel;
    this.view = new UrlbarView(this);
    this.valueIsTyped = false;
    this.userInitiatedFocus = false;

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
  /* eslint-disable require-jsdoc */

  _oninput(event) {
    this.openResults();
  }
}
