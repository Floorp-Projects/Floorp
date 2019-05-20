/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReflectedFluentElement */

class LoginItem extends ReflectedFluentElement {
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

    this.reflectFluentStrings();

    for (let selector of [
      ".copy-password-button",
      ".copy-username-button",
      ".delete-button",
      ".edit-button",
      ".open-site-button",
      ".save-changes-button",
      ".cancel-button",
    ]) {
      let button = this.shadowRoot.querySelector(selector);
      button.addEventListener("click", this);
    }

    window.addEventListener("AboutLoginsLoginSelected", this);

    let copyUsernameButton = this.shadowRoot.querySelector(".copy-username-button");
    let copyPasswordButton = this.shadowRoot.querySelector(".copy-password-button");
    copyUsernameButton.relatedInput = this.shadowRoot.querySelector("modal-input[name='username']");
    copyPasswordButton.relatedInput = this.shadowRoot.querySelector("modal-input[name='password']");

    this.render();
  }

  static get reflectedFluentIDs() {
    return [
      "cancel-button",
      "copied-password-button",
      "copied-username-button",
      "copy-password-button",
      "copy-username-button",
      "delete-button",
      "edit-button",
      "hostname-label",
      "modal-input-reveal-checkbox-hide",
      "modal-input-reveal-checkbox-show",
      "open-site-button",
      "password-label",
      "save-changes-button",
      "time-created",
      "time-changed",
      "time-used",
      "username-label",
    ];
  }

  static get observedAttributes() {
    return this.reflectedFluentIDs;
  }

  handleSpecialCaseFluentString(attrName) {
    switch (attrName) {
      case "copied-password-button":
      case "copy-password-button": {
        let copyPasswordButton = this.shadowRoot.querySelector(".copy-password-button");
        let newAttrName = attrName.substr(0, attrName.indexOf("-")) + "-button-text";
        copyPasswordButton.setAttribute(newAttrName, this.getAttribute(attrName));
        break;
      }
      case "copied-username-button":
      case "copy-username-button": {
        let copyUsernameButton = this.shadowRoot.querySelector(".copy-username-button");
        let newAttrName = attrName.substr(0, attrName.indexOf("-")) + "-button-text";
        copyUsernameButton.setAttribute(newAttrName, this.getAttribute(attrName));
        break;
      }
      case "modal-input-reveal-checkbox-hide": {
        this.shadowRoot.querySelector("modal-input[name='password']")
                       .setAttribute("reveal-checkbox-hide", this.getAttribute(attrName));
        break;
      }
      case "modal-input-reveal-checkbox-show": {
        this.shadowRoot.querySelector("modal-input[name='password']")
                       .setAttribute("reveal-checkbox-show", this.getAttribute(attrName));
        break;
      }
      default:
        return false;
    }
    return true;
  }

  render() {
    let l10nArgs = {
      timeCreated: this._login.timeCreated || "",
      timeChanged: this._login.timePasswordChanged || "",
      timeUsed: this._login.timeLastUsed || "",
    };
    document.l10n.setAttributes(this, "login-item", l10nArgs);
    let hostnameNoScheme = this._login.hostname && new URL(this._login.hostname).hostname;
    this.shadowRoot.querySelector(".title").textContent = hostnameNoScheme || "";
    this.shadowRoot.querySelector(".hostname").textContent = this._login.hostname || "";
    this.shadowRoot.querySelector("modal-input[name='username']").setAttribute("value", this._login.username || "");
    this.shadowRoot.querySelector("modal-input[name='password']").setAttribute("value", this._login.password || "");
  }

  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsLoginSelected": {
        this.setLogin(event.detail);
        break;
      }
      case "click": {
        if (event.target.classList.contains("cancel-button")) {
          this.toggleEditing();
          this.render();
          return;
        }
        if (event.target.classList.contains("copy-password-button")) {
          return;
        }
        if (event.target.classList.contains("copy-username-button")) {
          return;
        }
        if (event.target.classList.contains("delete-button")) {
          document.dispatchEvent(new CustomEvent("AboutLoginsDeleteLogin", {
            bubbles: true,
            detail: this._login,
          }));
          return;
        }
        if (event.target.classList.contains("edit-button")) {
          this.toggleEditing();
          return;
        }
        if (event.target.classList.contains("open-site-button")) {
          document.dispatchEvent(new CustomEvent("AboutLoginsOpenSite", {
            bubbles: true,
            detail: this._login,
          }));
          return;
        }
        if (event.target.classList.contains("save-changes-button")) {
          let loginUpdates = {
            guid: this._login.guid,
          };
          let formUsername = this.shadowRoot.querySelector("modal-input[name='username']").value.trim();
          if (formUsername != this._login.username) {
            loginUpdates.username = formUsername;
          }
          let formPassword = this.shadowRoot.querySelector("modal-input[name='password']").value.trim();
          if (formPassword != this._login.password) {
            loginUpdates.password = formPassword;
          }
          document.dispatchEvent(new CustomEvent("AboutLoginsUpdateLogin", {
            bubbles: true,
            detail: loginUpdates,
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

  loginModified(login) {
    if (login.guid != this._login.guid) {
      return;
    }

    this._login = login;
    this.toggleEditing(false);
    this.render();
  }

  loginRemoved(login) {
    if (login.guid != this._login.guid) {
      return;
    }
    this._login = {};
    this.render();
  }

  toggleEditing(force) {
    let shouldEdit = force !== undefined ? force : !this.hasAttribute("editing");
    this.shadowRoot.querySelector(".edit-button").disabled = shouldEdit;
    this.shadowRoot.querySelectorAll("modal-input")
                   .forEach(el => el.toggleAttribute("editing", shouldEdit));
    this.toggleAttribute("editing", shouldEdit);
  }
}
customElements.define("login-item", LoginItem);
