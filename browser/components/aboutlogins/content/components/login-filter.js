/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ReflectedFluentElement from "chrome://browser/content/aboutlogins/components/reflected-fluent-element.js";

export default class LoginFilter extends ReflectedFluentElement {
  connectedCallback() {
    if (this.children.length) {
      return;
    }

    let loginFilterTemplate = document.querySelector("#login-filter-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginFilterTemplate.content.cloneNode(true));

    this.reflectFluentStrings();

    this.addEventListener("input", this);
  }

  handleEvent(event) {
    switch (event.type) {
      case "input": {
        this.dispatchFilterEvent(event.originalTarget.value);
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
    return this.shadowRoot.querySelector("input").value;
  }

  set value(val) {
    this.shadowRoot.querySelector("input").value = val;
    this.dispatchFilterEvent(val);
  }

  handleSpecialCaseFluentString(attrName) {
    if (attrName != "placeholder") {
      return false;
    }

    this.shadowRoot.querySelector("input").placeholder = this.getAttribute(attrName);
    return true;
  }

  dispatchFilterEvent(value) {
    this.dispatchEvent(new CustomEvent("AboutLoginsFilterLogins", {
      bubbles: true,
      composed: true,
      detail: value,
    }));
  }
}
customElements.define("login-filter", LoginFilter);
