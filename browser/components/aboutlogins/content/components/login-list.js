/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ReflectedFluentElement, LoginListItem */

class LoginList extends ReflectedFluentElement {
  constructor() {
    super();
    this._logins = [];
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
    for (let login of this._logins) {
      list.append(new LoginListItem(login));
    }
    document.l10n.setAttributes(this, "login-list", {count: this._logins.length});
  }

  handleEvent(event) {
    switch (event.type) {
      case "AboutLoginsFilterLogins": {
        let query = event.detail.toLocaleLowerCase();
        let matchingLoginGuids;
        if (query) {
          matchingLoginGuids = this._logins.filter(login => {
            return login.hostname.toLocaleLowerCase().includes(query) ||
                   login.username.toLocaleLowerCase().includes(query);
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
        document.l10n.setAttributes(this, "login-list", {count: matchingLoginGuids.length});
        break;
      }
      case "AboutLoginsLoginSelected": {
        if (this._selectedItem) {
          if (this._selectedItem.getAttribute("guid") == event.detail.guid) {
            return;
          }
          this._selectedItem.classList.toggle("selected", false);
        }
        this._selectedItem = this.shadowRoot.querySelector(`login-list-item[guid="${event.detail.guid}"]`);
        this._selectedItem.classList.toggle("selected", true);
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

  setLogins(logins) {
    let list = this.shadowRoot.querySelector("ol");
    list.textContent = "";
    this._logins = logins;
    this.render();
  }

  loginAdded(login) {
    this._logins.push(login);
    let list = this.shadowRoot.querySelector("ol");
    list.append(new LoginListItem(login));
  }

  loginModified(login) {
    for (let i = 0; i < this._logins.length; i++) {
      if (this._logins[i].guid == login.guid) {
        this._logins[i] = login;
        break;
      }
    }
    let list = this.shadowRoot.querySelector("ol");
    for (let loginListItem of list.children) {
      if (loginListItem.getAttribute("guid") == login.guid) {
        loginListItem.update(login);
        break;
      }
    }
  }

  loginRemoved(login) {
    this._logins = this._logins.filter(l => l.guid != login.guid);
    let list = this.shadowRoot.querySelector("ol");
    for (let loginListItem of list.children) {
      if (loginListItem.getAttribute("guid") == login.guid) {
        loginListItem.remove();
        break;
      }
    }
  }
}
customElements.define("login-list", LoginList);
