/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {recordTelemetryEvent} from "../aboutLoginsUtils.js";
import ReflectedFluentElement from "./reflected-fluent-element.js";

export default class LoginFilter extends ReflectedFluentElement {
  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }

    let loginFilterTemplate = document.querySelector("#login-filter-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginFilterTemplate.content.cloneNode(true));

    this._input = this.shadowRoot.querySelector("input");

    this.addEventListener("input", this);

    super.connectedCallback();
  }

  handleEvent(event) {
    switch (event.type) {
      case "input": {
        this._dispatchFilterEvent(event.originalTarget.value);
        break;
      }
    }
  }

  static get reflectedFluentIDs() {
    return ["placeholder"];
  }

  static get observedAttributes() {
    return this.reflectedFluentIDs;
  }

  get value() {
    return this._input.value;
  }

  set value(val) {
    this._input.value = val;
    this._dispatchFilterEvent(val);
  }

  handleSpecialCaseFluentString(attrName) {
    if (!this.shadowRoot ||
        attrName != "placeholder") {
      return false;
    }

    this._input.placeholder = this.getAttribute(attrName);
    return true;
  }

  _dispatchFilterEvent(value) {
    this.dispatchEvent(new CustomEvent("AboutLoginsFilterLogins", {
      bubbles: true,
      composed: true,
      detail: value,
    }));

    recordTelemetryEvent({object: "list", method: "filter"});
  }
}
customElements.define("login-filter", LoginFilter);
