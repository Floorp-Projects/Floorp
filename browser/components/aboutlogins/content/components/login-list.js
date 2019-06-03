/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import LoginListItem from "chrome://browser/content/aboutlogins/components/login-list-item.js";
import ReflectedFluentElement from "chrome://browser/content/aboutlogins/components/reflected-fluent-element.js";

export default class LoginList extends ReflectedFluentElement {
  constructor() {
    super();
    this._logins = [];
    this._filter = "";
    this._selectedItem = null;
  }

  connectedCallback() {
    if (this.children.length) {
      return;
    }
    let loginListTemplate = document.querySelector("#login-list-template");
    this.attachShadow({mode: "open"})
        .appendChild(loginListTemplate.content.cloneNode(true));

    this.reflectFluentStrings();

    this.render();

    window.addEventListener("AboutLoginsLoginSelected", this);
    window.addEventListener("AboutLoginsFilterLogins", this);
  }

  render() {
    let list = this.shadowRoot.querySelector("ol");
    list.textContent = "";
    for (let login of this._logins) {
      list.append(new LoginListItem(login));
    }

    let visibleLoginCount = this._applyFilter();
    document.l10n.setAttributes(this, "login-list", {count: visibleLoginCount});
  }

  _applyFilter() {
    let matchingLoginGuids;
    if (this._filter) {
      matchingLoginGuids = this._logins.filter(login => {
        return login.hostname.toLocaleLowerCase().includes(this._filter) ||
               login.username.toLocaleLowerCase().includes(this._filter);
      }).map(login => login.guid);
    } else {
      matchingLoginGuids = this._logins.map(login => login.guid);
    }

    for (let listItem of this.shadowRoot.querySelectorAll("login-list-item")) {
      if (matchingLoginGuids.includes(listItem.getAttribute("guid"))) {
        if (listItem.hidden) {
          listItem.hidden = false;
        }
      } else if (!listItem.hidden) {
        listItem.hidden = true;
      }
    }

    return matchingLoginGuids.length;
  }

  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsFilterLogins": {
        this._filter = event.detail.toLocaleLowerCase();
        this.render();
        break;
      }
      case "AboutLoginsLoginSelected": {
        if (this._selectedItem) {
          if (this._selectedItem.getAttribute("guid") == event.detail.guid) {
            return;
          }
          this._selectedItem.classList.remove("selected");
        }
        this._selectedItem = this.shadowRoot.querySelector(`login-list-item[guid="${event.detail.guid}"]`);
        this._selectedItem.classList.add("selected");
        break;
      }
    }
  }

  static get reflectedFluentIDs() {
    return ["count"];
  }

  static get observedAttributes() {
    return this.reflectedFluentIDs;
  }

  clearSelection() {
    if (!this._selectedItem) {
      return;
    }
    this._selectedItem.classList.remove("selected");
    this._selectedItem = null;
  }

  setLogins(logins) {
    this._logins = logins;
    this.render();
  }

  loginAdded(login) {
    this._logins.push(login);
    this.render();
  }

  loginModified(login) {
    for (let i = 0; i < this._logins.length; i++) {
      if (this._logins[i].guid == login.guid) {
        this._logins[i] = login;
        break;
      }
    }
    this.render();
  }

  loginRemoved(login) {
    this._logins = this._logins.filter(l => l.guid != login.guid);
    this.render();
  }
}
customElements.define("login-list", LoginList);
