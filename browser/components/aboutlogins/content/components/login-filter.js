/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { recordTelemetryEvent } from "../aboutLoginsUtils.js";

export default class LoginFilter extends HTMLElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }

    let loginFilterTemplate = document.querySelector("#login-filter-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(loginFilterTemplate.content.cloneNode(true));

    this._input = this.shadowRoot.querySelector("input");

    this.addEventListener("input", this);
    window.addEventListener("AboutLoginsFilterLogins", this);
  }

  focus() {
    this._input.focus();
  }

  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsFilterLogins": {
        if (this.value != event.detail) {
          this.value = event.detail;
        }
        break;
      }
      case "input": {
        this._dispatchFilterEvent(event.originalTarget.value);
        break;
      }
    }
  }

  get value() {
    return this._input.value;
  }

  set value(val) {
    this._input.value = val;
    this._dispatchFilterEvent(val);
  }

  _dispatchFilterEvent(value) {
    this.dispatchEvent(
      new CustomEvent("AboutLoginsFilterLogins", {
        bubbles: true,
        composed: true,
        detail: value,
      })
    );

    recordTelemetryEvent({ object: "list", method: "filter" });
  }
}
customElements.define("login-filter", LoginFilter);
