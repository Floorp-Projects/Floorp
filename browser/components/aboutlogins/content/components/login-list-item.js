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

    this._title = this.shadowRoot.querySelector(".title");
    this._username = this.shadowRoot.querySelector(".username");

    this.render();

    this.addEventListener("click", this);
  }

  render() {
    if (!this._login.guid) {
      delete this.dataset.guid;
      this._title.textContent = this.getAttribute("new-login-title");
      this._username.textContent = this.getAttribute("new-login-subtitle");
      return;
    }

    this.dataset.guid = this._login.guid;
    this._title.textContent = this._login.title;
    this._username.textContent = this._login.username.trim() || this.getAttribute("missing-username");
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

  /**
   * Updates the cached login object with new values.
   *
   * @param {login} login The login object to display. The login object is
   *                      a plain JS object representation of nsILoginInfo/nsILoginMetaInfo.
   */
  update(login) {
    this._login = login;
    this.render();
  }
}
customElements.define("login-list-item", LoginListItem);
