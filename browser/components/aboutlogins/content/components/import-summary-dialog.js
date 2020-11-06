/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setKeyboardAccessForNonDialogElements } from "../aboutLoginsUtils.js";

export default class ImportSummaryDialog extends HTMLElement {
  constructor() {
    super();
    this._promise = null;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let template = document.querySelector("#import-summary-dialog-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(template.content.cloneNode(true));

    this._dismissButton = this.shadowRoot.querySelector(".import-done-button");
    this._overlay = this.shadowRoot.querySelector(".overlay");

    this._added = this.shadowRoot.querySelector(".import-items-added");
    this._modified = this.shadowRoot.querySelector(".import-items-modified");
    this._noChange = this.shadowRoot.querySelector(".import-items-no-change");
    this._error = this.shadowRoot.querySelector(".import-items-errors");
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
          event.currentTarget.classList.contains("import-done-button") ||
          event.target.classList.contains("overlay")
        ) {
          this.onCancel();
        }
    }
  }

  hide() {
    setKeyboardAccessForNonDialogElements(true);
    this._dismissButton.removeEventListener("click", this);
    this._overlay.removeEventListener("click", this);
    window.removeEventListener("keydown", this);

    this.hidden = true;
  }

  show({ logins }) {
    const report = {
      added: 0,
      modified: 0,
      no_change: 0,
      error: 0,
    };
    for (let loginRow of logins) {
      if (loginRow.result.indexOf("error") > -1) {
        report.error++;
      } else {
        report[loginRow.result]++;
      }
    }
    this._updateCount(
      report.added,
      this._added,
      "about-logins-import-dialog-items-added"
    );
    this._updateCount(
      report.modified,
      this._modified,
      "about-logins-import-dialog-items-modified"
    );
    this._updateCount(
      report.no_change,
      this._noChange,
      "about-logins-import-dialog-items-no-change"
    );
    this._updateCount(
      report.error,
      this._error,
      "about-logins-import-dialog-items-error"
    );
    setKeyboardAccessForNonDialogElements(false);
    this.hidden = false;

    this._dismissButton.addEventListener("click", this);
    this._overlay.addEventListener("click", this);
    window.addEventListener("keydown", this);

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

  _updateCount(count, component, message) {
    if (count != document.l10n.getAttributes(component).args.count) {
      document.l10n.setAttributes(component, message, { count });
    }
  }
}
customElements.define("import-summary-dialog", ImportSummaryDialog);
