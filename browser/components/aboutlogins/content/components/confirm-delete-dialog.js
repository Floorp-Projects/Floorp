/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ReflectedFluentElement from "./reflected-fluent-element.js";

export default class ConfirmDeleteDialog extends ReflectedFluentElement {
  constructor() {
    super();
    this._promise = null;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let template = document.querySelector("#confirm-delete-dialog-template");
    this.attachShadow({mode: "open"})
        .appendChild(template.content.cloneNode(true));

    this._cancelButton = this.shadowRoot.querySelector(".cancel-button");
    this._confirmButton = this.shadowRoot.querySelector(".confirm-button");
    this._dismissButton = this.shadowRoot.querySelector(".dismiss-button");
    this._message = this.shadowRoot.querySelector(".message");
    this._overlay = this.shadowRoot.querySelector(".overlay");
    this._title = this.shadowRoot.querySelector(".title");

    super.connectedCallback();
  }

  static get reflectedFluentIDs() {
    return [
      "title",
      "message",
      "cancel-button",
      "confirm-button",
    ];
  }

  static get observedAttributes() {
    return this.reflectedFluentIDs;
  }

  handleEvent(event) {
    switch (event.type) {
      case "keydown":
        if (event.key === "Escape" && !event.defaultPrevented) {
          this.onCancel();
        }
        break;
      case "click":
        if (event.target.classList.contains("cancel-button") ||
            event.target.classList.contains("dismiss-button") ||
            event.target.classList.contains("overlay")) {
          this.onCancel();
        } else if (event.target.classList.contains("confirm-button")) {
          this.onConfirm();
        }
    }
  }

  hide() {
    this._cancelButton.removeEventListener("click", this);
    this._confirmButton.removeEventListener("click", this);
    this._dismissButton.removeEventListener("click", this);
    this._overlay.removeEventListener("click", this);
    window.removeEventListener("keydown", this);

    this.hidden = true;
  }

  show() {
    this.hidden = false;

    this._cancelButton.addEventListener("click", this);
    this._confirmButton.addEventListener("click", this);
    this._dismissButton.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    window.addEventListener("keydown", this);

    // For accessibility, focus the least destructive action button when the
    // dialog loads.
    this._cancelButton.focus();

    this._promise = new Promise((resolve, reject) => {
      this._resolve = resolve;
      this._reject = reject;
    });

    return this._promise;
  }

  onCancel() {
    this._reject();
    this.hide();
  }

  onConfirm() {
    this._resolve();
    this.hide();
  }
}
customElements.define("confirm-delete-dialog", ConfirmDeleteDialog);
