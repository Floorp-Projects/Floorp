/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {recordTelemetryEvent} from "../aboutLoginsUtils.js";

export default class LoginListItem extends HTMLElement {
  constructor(login) {
    super();
    this._login = login;
  }

  connectedCallback() {
    if (this.shadowRoot) {
      this.render();
      return;
    }

    let loginListItemTemplate = document.querySelector("#login-list-item-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginListItemTemplate.content.cloneNode(true));
    this.render();

    this.addEventListener("click", this);
  }

  render() {
    let origin = this.shadowRoot.querySelector(".origin");
    let username = this.shadowRoot.querySelector(".username");

    if (!this._login.guid) {
      this.removeAttribute("guid");
      origin.textContent = this.getAttribute("new-login-title");
      username.textContent = this.getAttribute("new-login-subtitle");
      return;
    }

    this.setAttribute("guid", this._login.guid);
    origin.textContent = this._login.origin;
    username.textContent = this._login.username.trim() || this.getAttribute("missing-username");
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.dispatchEvent(new CustomEvent("AboutLoginsLoginSelected", {
          bubbles: true,
          composed: true,
          detail: this._login,
        }));

        recordTelemetryEvent({object: "existing_login", method: "select"});
      }
    }
  }

  update(login) {
    this._login = login;
    this.render();
  }
}
customElements.define("login-list-item", LoginListItem);
