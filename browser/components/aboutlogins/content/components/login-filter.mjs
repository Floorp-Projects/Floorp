/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { recordTelemetryEvent } from "../aboutLoginsUtils.mjs";

export default class LoginFilter extends HTMLElement {
  get #loginList() {
    return document.querySelector("login-list");
  }

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
    this._input.addEventListener("keydown", this);
    window.addEventListener("AboutLoginsFilterLogins", this);
  }

  focus() {
    this._input.focus();
  }

  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsFilterLogins":
        this.#filterLogins(event.detail);
        break;
      case "input":
        this.#input(event.originalTarget.value);
        break;
      case "keydown":
        this.#keyDown(event);
        break;
    }
  }

  #filterLogins(filterText) {
    if (this.value != filterText) {
      this.value = filterText;
    }
  }

  #input(value) {
    this._dispatchFilterEvent(value);
  }

  #keyDown(e) {
    switch (e.code) {
      case "ArrowUp":
        e.preventDefault();
        this.#loginList.selectPrevious();
        break;
      case "ArrowDown":
        e.preventDefault();
        this.#loginList.selectNext();
        break;
      case "Escape":
        e.preventDefault();
        this.value = "";
        break;
      case "Enter":
        e.preventDefault();
        this.#loginList.clickSelected();
        break;
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
