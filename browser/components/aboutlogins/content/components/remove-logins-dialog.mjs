/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setKeyboardAccessForNonDialogElements } from "../aboutLoginsUtils.mjs";

export default class RemoveLoginsDialog extends HTMLElement {
  constructor() {
    super();
    this._promise = null;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let template = document.querySelector("#remove-logins-dialog-template");
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
    this._checkbox = this.shadowRoot.querySelector(".checkbox");
    this._checkboxLabel = this.shadowRoot.querySelector(".checkbox-text");
  }

  handleEvent(event) {
    switch (event.type) {
      case "keydown":
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
        } else if (event.target.classList.contains("checkbox")) {
          this._confirmButton.disabled = !this._checkbox.checked;
        }
    }
  }

  hide() {
    setKeyboardAccessForNonDialogElements(true);
    this._cancelButton.removeEventListener("click", this);
    this._confirmButton.removeEventListener("click", this);
    this._dismissButton.removeEventListener("click", this);
    this._overlay.removeEventListener("click", this);
    this._checkbox.removeEventListener("click", this);
    window.removeEventListener("keydown", this);

    this._checkbox.checked = false;

    this.hidden = true;
  }

  show({ title, message, confirmButtonLabel, confirmCheckboxLabel, count }) {
    setKeyboardAccessForNonDialogElements(false);
    this.hidden = false;

    document.l10n.setAttributes(this._title, title, {
      count,
    });
    document.l10n.setAttributes(this._message, message, {
      count,
    });
    document.l10n.setAttributes(this._confirmButton, confirmButtonLabel, {
      count,
    });
    document.l10n.setAttributes(this._checkboxLabel, confirmCheckboxLabel, {
      count,
    });

    this._checkbox.addEventListener("click", this);
    this._cancelButton.addEventListener("click", this);
    this._confirmButton.addEventListener("click", this);
    this._dismissButton.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    window.addEventListener("keydown", this);

    this._confirmButton.disabled = true;
    // For speed-of-use, focus the confirmation checkbox when the dialog loads.
    // Introducing this checkbox provides enough of a buffer for accidental deletions.
    this._checkbox.focus();

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

customElements.define("remove-logins-dialog", RemoveLoginsDialog);
