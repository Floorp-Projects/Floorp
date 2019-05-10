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

    for (let selector of [
      ".delete-button",
      ".save-changes-button",
      ".cancel-button",
    ]) {
      let button = this.shadowRoot.querySelector(selector);
      button.addEventListener("click", this);
    }

    window.addEventListener("AboutLoginsLoginSelected", this);

    this.render();
  }

  static get observedAttributes() {
    return [
      "cancel-button",
      "delete-button",
      "hostname-label",
      "password-label",
      "save-changes-button",
      "time-created",
      "time-changed",
      "time-used",
      "username-label",
    ];
  }

  /* Fluent doesn't handle localizing into Shadow DOM yet so strings
     need to get reflected in to their targeted element. */
  attributeChangedCallback(attr, oldValue, newValue) {
    if (!this.shadowRoot) {
      return;
    }

    // Strings that are reflected to their shadowed element are assigned
    // to an attribute name that matches a className on the element.
    let shadowedElement = this.shadowRoot.querySelector("." + attr);
    shadowedElement.textContent = newValue;
  }

  render() {
    let l10nArgs = {
      timeCreated: this._login.timeCreated || "",
      timeChanged: this._login.timePasswordChanged || "",
      timeUsed: this._login.timeLastUsed || "",
    };
    document.l10n.setAttributes(this, "login-item", l10nArgs);
    let hostnameNoScheme = this._login.hostname && new URL(this._login.hostname).hostname;
    this.shadowRoot.querySelector(".header").textContent = hostnameNoScheme || "";
    this.shadowRoot.querySelector(".hostname").textContent = this._login.hostname || "";
    this.shadowRoot.querySelector("input[name='username']").value = this._login.username || "";
    this.shadowRoot.querySelector("input[name='password']").value = this._login.password || "";
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
          return;
        }
        if (event.target.classList.contains("save-changes-button")) {
          let loginUpdates = {
            guid: this._login.guid,
          };
          let formUsername = this.shadowRoot.querySelector("input[name='username']").value.trim();
          if (formUsername != this._login.username) {
            loginUpdates.username = formUsername;
          }
          let formPassword = this.shadowRoot.querySelector("input[name='password']").value.trim();
          if (formPassword != this._login.password) {
            loginUpdates.password = formPassword;
          }
          document.dispatchEvent(new CustomEvent("AboutLoginsUpdateLogin", {
            bubbles: true,
            detail: loginUpdates,
          }));
          return;
        }
        if (event.target.classList.contains("cancel-button")) {
          this.render();
        }
        break;
      }
    }
  }

  setLogin(login) {
    this._login = login;
    this.render();
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
