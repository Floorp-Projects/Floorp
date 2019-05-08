/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class LoginItem extends HTMLElement {
  constructor() {
    super();
    this._login = {};
  }

  connectedCallback() {
    if (this.children.length) {
      this.render();
      return;
    }

    let loginItemTemplate = document.querySelector("#login-item-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginItemTemplate.content.cloneNode(true));

    let deleteButton = this.shadowRoot.querySelector(".delete-button");
    deleteButton.addEventListener("click", this);

    window.addEventListener("AboutLoginsLoginSelected", this);

    this.render();
  }

  static get observedAttributes() {
    return [
      "login-item-delete",
      "login-item-hostname",
      "login-item-password",
      "login-item-time-created",
      "login-item-username",
    ];
  }

  /* Fluent doesn't handle localizing into Shadow DOM yet so strings
     need to get reflected in to their targeted element. */
  attributeChangedCallback(attr, oldValue, newValue) {
    if (!this.shadowRoot) {
      return;
    }

    switch (attr) {
      case "login-item-delete":
        this.shadowRoot.querySelector(".delete-button").textContent = newValue;
        break;
      case "login-item-hostname":
        this.shadowRoot.querySelector(".hostname-label").textContent = newValue;
        break;
      case "login-item-password":
        this.shadowRoot.querySelector(".password-label").textContent = newValue;
        break;
      case "login-item-time-created":
        this.shadowRoot.querySelector(".time-created-label").textContent = newValue;
        break;
      case "login-item-username":
        this.shadowRoot.querySelector(".username-label").textContent = newValue;
        break;
    }
  }

  render() {
    this.shadowRoot.querySelector("input[name='hostname']").value = this._login.hostname || "";
    this.shadowRoot.querySelector("input[name='username']").value = this._login.username || "";
    this.shadowRoot.querySelector("input[name='password']").value = this._login.password || "";
    this.shadowRoot.querySelector(".time-created").textContent = this._login.timeCreated || "";
  }

  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsLoginSelected": {
        this.setLogin(event.detail);
        break;
      }
      case "click": {
        if (event.target.classList.contains("delete-button")) {
          document.dispatchEvent(new CustomEvent("AboutLoginsDeleteLogin", {
            bubbles: true,
            detail: this._login,
          }));
        }
        break;
      }
    }
  }

  setLogin(login) {
    this._login = login;
    this.render();
  }

  loginAdded(login) {
    if (!this._login.guid) {
      let tempLogin = {
        username: this.shadowRoot.querySelector("input[name='username']").value,
        formSubmitURL: "", // Use the wildcard since the user doesn't supply it.
        hostname: this.shadowRoot.querySelector("input[name='hostname']").value,
        password: this.shadowRoot.querySelector("input[name='password']").value,
      };
      // Need to use LoginHelper.doLoginsMatch() to see if the login
      // that was added is the login that was being edited, so we
      // can update time-created, etc.
      if (window.AboutLoginsUtils.doLoginsMatch(tempLogin, login)) {
        this._login = login;
        this.render();
      }
    } else if (login.guid == this._login.guid) {
      this._login = login;
      this.render();
    }
  }

  loginModified(login) {
    if (login.guid != this._login.guid) {
      return;
    }

    this._login = login;
    this.render();
  }

  loginRemoved(login) {
    if (login.guid != this._login.guid) {
      return;
    }
    this._login = {};
    this.render();
  }
}
customElements.define("login-item", LoginItem);
