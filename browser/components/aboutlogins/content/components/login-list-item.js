/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {recordTelemetryEvent} from "../aboutLoginsUtils.js";

export default class LoginListItem extends HTMLElement {
  constructor(login) {
    super();
    this._login = login;
    this.id = login.guid ?
      // Prepend the ID with a string since IDs must not begin with a number.
      "lli-" + this._login.guid :
      "new-login-list-item";
  }

  connectedCallback() {
    if (this.shadowRoot) {
      this.render();
      return;
    }

    let loginListItemTemplate = document.querySelector("#login-list-item-template");
    let shadowRoot = this.attachShadow({mode: "open"});
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(loginListItemTemplate.content.cloneNode(true));

    this._title = this.shadowRoot.querySelector(".title");
    this._username = this.shadowRoot.querySelector(".username");
    this.setAttribute("role", "option");

    this.addEventListener("click", this);

    this.render();
  }

  render() {
    if (!this._login.guid) {
      delete this.dataset.guid;
      document.l10n.setAttributes(this._title, "login-list-item-title-new-login");
      document.l10n.setAttributes(this._username, "login-list-item-subtitle-new-login");
      return;
    }

    this.dataset.guid = this._login.guid;
    this._title.textContent = this._login.title;
    if (this._login.username.trim()) {
      this._username.removeAttribute("data-l10n-id");
      this._username.textContent = this._login.username.trim();
    } else {
      document.l10n.setAttributes(this._username, "login-list-item-subtitle-missing-username");
    }
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
