/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class LoginFilter extends HTMLElement {
  connectedCallback() {
    if (this.children.length) {
      return;
    }

    let loginFilterTemplate = document.querySelector("#login-filter-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginFilterTemplate.content.cloneNode(true));

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

  static get observedAttributes() {
    return ["placeholder"];
  }

  /* Fluent doesn't handle localizing into Shadow DOM yet so strings
     need to get reflected in to their targeted element. */
  attributeChangedCallback(attr, oldValue, newValue) {
    if (!this.shadowRoot) {
      return;
    }

    switch (attr) {
      case "placeholder":
        this.shadowRoot.querySelector("input").placeholder = newValue;
        break;
    }
  }
}
customElements.define("login-filter", LoginFilter);
