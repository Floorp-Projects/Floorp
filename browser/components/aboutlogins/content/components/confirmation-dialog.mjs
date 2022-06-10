/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setKeyboardAccessForNonDialogElements } from "../aboutLoginsUtils.mjs";

export default class ConfirmationDialog extends HTMLElement {
  constructor() {
    super();
    this._promise = null;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let template = document.querySelector("#confirmation-dialog-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(template.content.cloneNode(true));

    this._buttons = this.shadowRoot.querySelector(".buttons");
    this._cancelButton = this.shadowRoot.querySelector(".cancel-button");
    this._confirmButton = this.shadowRoot.querySelector(".confirm-button");
    this._dismissButton = this.shadowRoot.querySelector(".dismiss-button");
    this._message = this.shadowRoot.querySelector(".message");
    this._overlay = this.shadowRoot.querySelector(".overlay");
    this._title = this.shadowRoot.querySelector(".title");

    this._buttons.classList.toggle("macosx", navigator.platform == "MacIntel");
  }

  handleEvent(event) {
    switch (event.type) {
      case "keydown":
        if (event.repeat) {
          // Prevent repeat keypresses from accidentally confirming the
          // dialog since the confirmation button is focused by default.
          event.preventDefault();
          return;
        }
        if (event.key === "Escape" && !event.defaultPrevented) {
          this.onCancel();
        }
        break;
      case "click":
        if (
          event.target.classList.contains("cancel-button") ||
          event.currentTarget.classList.contains("dismiss-button") ||
          event.target.classList.contains("overlay")
        ) {
          this.onCancel();
        } else if (event.target.classList.contains("confirm-button")) {
          this.onConfirm();
        }
    }
  }

  hide() {
    setKeyboardAccessForNonDialogElements(true);
    this._cancelButton.removeEventListener("click", this);
    this._confirmButton.removeEventListener("click", this);
    this._dismissButton.removeEventListener("click", this);
    this._overlay.removeEventListener("click", this);
    window.removeEventListener("keydown", this);

    this.hidden = true;
  }

  show({ title, message, confirmButtonLabel }) {
    setKeyboardAccessForNonDialogElements(false);
    this.hidden = false;

    document.l10n.setAttributes(this._title, title);
    document.l10n.setAttributes(this._message, message);
    document.l10n.setAttributes(this._confirmButton, confirmButtonLabel);

    this._cancelButton.addEventListener("click", this);
    this._confirmButton.addEventListener("click", this);
    this._dismissButton.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    window.addEventListener("keydown", this);

    // For speed-of-use, focus the confirm button when the
    // dialog loads. Showing the dialog itself provides enough
    // of a buffer for accidental deletions.
    this._confirmButton.focus();

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
customElements.define("confirmation-dialog", ConfirmationDialog);
