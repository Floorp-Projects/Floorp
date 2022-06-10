/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  setKeyboardAccessForNonDialogElements,
  initDialog,
} from "../aboutLoginsUtils.mjs";

export default class GenericDialog extends HTMLElement {
  constructor() {
    super();
    this._promise = null;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    const shadowRoot = initDialog(this, "#generic-dialog-template");
    this._dismissButton = this.querySelector(".dismiss-button");
    this._overlay = shadowRoot.querySelector(".overlay");
  }

  handleEvent(event) {
    switch (event.type) {
      case "keydown":
        if (event.key === "Escape" && !event.defaultPrevented) {
          this.hide();
        }
        break;
      case "click":
        if (
          event.currentTarget.classList.contains("dismiss-button") ||
          event.target.classList.contains("overlay")
        ) {
          this.hide();
        }
    }
  }

  show() {
    setKeyboardAccessForNonDialogElements(false);
    this.hidden = false;
    this.parentNode.host.hidden = false;

    this._dismissButton.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    window.addEventListener("keydown", this);
  }

  hide() {
    setKeyboardAccessForNonDialogElements(true);
    this._dismissButton.removeEventListener("click", this);
    this._overlay.removeEventListener("click", this);
    window.removeEventListener("keydown", this);

    this.hidden = true;
    this.parentNode.host.hidden = true;
  }
}

customElements.define("generic-dialog", GenericDialog);
