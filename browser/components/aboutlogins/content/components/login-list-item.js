/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class LoginListItem extends HTMLElement {
  constructor(login) {
    super();
    this._login = login;
  }

  connectedCallback() {
    if (this.children.length) {
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
    this.setAttribute("guid", this._login.guid);
    this.shadowRoot.querySelector(".hostname").textContent = this._login.hostname;
    this.shadowRoot.querySelector(".username").textContent = this._login.username;
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.dispatchEvent(new CustomEvent("AboutLoginsLoginSelected", {
          bubbles: true,
          composed: true,
          detail: this._login,
        }));
      }
    }
  }

  update(login) {
    this._login = login;
    this.render();
  }
}
customElements.define("login-list-item", LoginListItem);
