/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals LoginListItem */

class LoginList extends HTMLElement {
  constructor() {
    super();
    this._logins = [];
  }

  connectedCallback() {
    if (this.children.length) {
      return;
    }
    let loginListTemplate = document.querySelector("#login-list-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginListTemplate.content.cloneNode(true));
    this.render();
  }

  render() {
    let pre = this.shadowRoot.querySelector("pre");
    for (let login of this._logins) {
      pre.append(new LoginListItem(login));
    }
  }

  static get observedAttributes() {
    return ["login-list-header"];
  }

  /* Fluent doesn't handle localizing into Shadow DOM yet so strings
     need to get reflected in to their targeted element. */
  attributeChangedCallback(attr, oldValue, newValue) {
    if (!this.shadowRoot) {
      return;
    }

    switch (attr) {
      case "login-list-header":
        this.shadowRoot.querySelector("h2").textContent = newValue;
        break;
    }
  }

  setLogins(logins) {
    let pre = this.shadowRoot.querySelector("pre");
    pre.textContent = "";
    this._logins = logins;
    this.render();
  }

  loginAdded(login) {
    this._logins.push(login);
    let pre = this.shadowRoot.querySelector("pre");
    pre.append(new LoginListItem(login));
  }

  loginModified(login) {
    for (let i = 0; i < this._logins.length; i++) {
      if (this._logins[i].guid == login.guid) {
        this._logins[i] = login;
        break;
      }
    }
    let pre = this.shadowRoot.querySelector("pre");
    for (let loginListItem of pre.children) {
      if (loginListItem.getAttribute("guid") == login.guid) {
        loginListItem.update(login);
        break;
      }
    }
  }

  loginRemoved(login) {
    this._logins = this._logins.filter(l => l.guid != login.guid);
    let pre = this.shadowRoot.querySelector("pre");
    for (let loginListItem of pre.children) {
      if (loginListItem.getAttribute("guid") == login.guid) {
        loginListItem.remove();
        break;
      }
    }
  }
}
customElements.define("login-list", LoginList);
