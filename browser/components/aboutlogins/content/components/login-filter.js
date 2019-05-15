/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReflectedFluentElement */

class LoginFilter extends ReflectedFluentElement {
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
        this.dispatchEvent(new CustomEvent("AboutLoginsFilterLogins", {
          bubbles: true,
          composed: true,
          detail: event.originalTarget.value,
        }));
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

  handleSpecialCaseFluentString(attrName) {
    if (attrName != "placeholder") {
      return false;
    }

    this.shadowRoot.querySelector("input").placeholder = this.getAttribute(attrName);
    return true;
  }
}
customElements.define("login-filter", LoginFilter);
