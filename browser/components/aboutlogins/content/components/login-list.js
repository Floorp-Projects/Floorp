/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import LoginListItem from "./login-list-item.js";

const collator = new Intl.Collator();
const sortFnOptions = {
  name: (a, b) => collator.compare(a.title, b.title),
  "last-used": (a, b) => (a.timeLastUsed < b.timeLastUsed),
  "last-changed": (a, b) => (a.timePasswordChanged < b.timePasswordChanged),
};

export default class LoginList extends HTMLElement {
  constructor() {
    super();
    this._logins = [];
    this._filter = "";
    this._selectedGuid = null;
    this._blankLoginListItem = new LoginListItem({});
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let loginListTemplate = document.querySelector("#login-list-template");
    let shadowRoot = this.attachShadow({mode: "open"});
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(loginListTemplate.content.cloneNode(true));

    this._list = this.shadowRoot.querySelector("ol");
    this._count = this.shadowRoot.querySelector(".count");

    this.render();

    this.shadowRoot.getElementById("login-sort")
                   .addEventListener("change", this);
    window.addEventListener("AboutLoginsLoginSelected", this);
    window.addEventListener("AboutLoginsFilterLogins", this);
    this.addEventListener("keydown", this);
  }

  render() {
    this._list.textContent = "";

    if (!this._logins.length) {
      document.l10n.setAttributes(this._count, "login-list-count", {count: 0});
      return;
    }

    if (!this._selectedGuid) {
      this._blankLoginListItem.classList.add("selected");
      this._blankLoginListItem.setAttribute("aria-selected", "true");
      this._list.setAttribute("aria-activedescendant", this._blankLoginListItem.id);
      this._list.append(this._blankLoginListItem);
    }

    for (let login of this._logins) {
      let listItem = new LoginListItem(login);
      if (login.guid == this._selectedGuid) {
        listItem.classList.add("selected");
        listItem.setAttribute("aria-selected", "true");
        this._list.setAttribute("aria-activedescendant", listItem.id);
      }
      this._list.append(listItem);
    }

    let visibleLoginCount = this._applyFilter();
    document.l10n.setAttributes(this._count, "login-list-count", {count: visibleLoginCount});
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        const sort = event.target.value;
        this._logins = this._logins.sort((a, b) => sortFnOptions[sort](a, b));
        this.render();
        break;
      }
      case "AboutLoginsFilterLogins": {
        this._filter = event.detail.toLocaleLowerCase();
        this.render();
        break;
      }
      case "AboutLoginsLoginSelected": {
        if (this._selectedGuid == event.detail.guid) {
          return;
        }

        this._selectedGuid = event.detail.guid || null;
        this.render();
        break;
      }
      case "keydown": {
        this._handleKeyboardNav(event);
        break;
      }
    }
  }

  /**
   * @param {login[]} logins An array of logins used for displaying in the list.
   */
  setLogins(logins) {
    this._logins = logins;
    this.render();
  }

  /**
   * @param {login} login A login that was added to storage.
   */
  loginAdded(login) {
    this._logins.push(login);
    this.render();
  }

  /**
   * @param {login} login A login that was modified in storage. The related login-list-item
   *                       will get updated.
   */
  loginModified(login) {
    for (let i = 0; i < this._logins.length; i++) {
      if (this._logins[i].guid == login.guid) {
        this._logins[i] = login;
        break;
      }
    }
    this.render();
  }

  /**
   * @param {login} login A login that was removed from storage. The related login-list-item
   *                      will get removed. The login object is a plain JS object
   *                      representation of nsILoginInfo/nsILoginMetaInfo.
   */
  loginRemoved(login) {
    this._logins = this._logins.filter(l => l.guid != login.guid);
    this.render();
  }

  /**
   * Filters the displayed logins in the list to only those matching the
   * cached filter value.
   */
  _applyFilter() {
    let matchingLoginGuids;
    if (this._filter) {
      matchingLoginGuids = this._logins.filter(login => {
        return login.origin.toLocaleLowerCase().includes(this._filter) ||
               login.username.toLocaleLowerCase().includes(this._filter);
      }).map(login => login.guid);
    } else {
      matchingLoginGuids = this._logins.map(login => login.guid);
    }

    for (let listItem of this._list.querySelectorAll("login-list-item")) {
      if (!listItem.dataset.guid) {
        // Don't hide the 'New Login' item if it is present.
        continue;
      }
      if (matchingLoginGuids.includes(listItem.dataset.guid)) {
        if (listItem.hidden) {
          listItem.hidden = false;
        }
      } else if (!listItem.hidden) {
        listItem.hidden = true;
      }
    }

    return matchingLoginGuids.length;
  }

  _handleKeyboardNav(event) {
    if (this._list != this.shadowRoot.activeElement) {
      return;
    }

    let isLTR = document.dir == "ltr";
    let activeDescendantId = this._list.getAttribute("aria-activedescendant");
    let activeDescendant = activeDescendantId ?
      this.shadowRoot.getElementById(activeDescendantId) :
      this._list.firstElementChild;
    let newlyFocusedItem = null;
    switch (event.key) {
      case "ArrowDown": {
        let nextItem = activeDescendant.nextElementSibling;
        if (!nextItem) {
          return;
        }
        newlyFocusedItem = nextItem;
        break;
      }
      case "ArrowLeft": {
        let item = isLTR ?
          activeDescendant.previousElementSibling :
          activeDescendant.nextElementSibling;
        if (!item) {
          return;
        }
        newlyFocusedItem = item;
        break;
      }
      case "ArrowRight": {
        let item = isLTR ?
          activeDescendant.nextElementSibling :
          activeDescendant.previousElementSibling;
        if (!item) {
          return;
        }
        newlyFocusedItem = item;
        break;
      }
      case "ArrowUp": {
        let previousItem = activeDescendant.previousElementSibling;
        if (!previousItem) {
          return;
        }
        newlyFocusedItem = previousItem;
        break;
      }
      case "Tab": {
        // Bug 1562716: Pressing Tab from the login-list cycles back to the
        // login-sort dropdown due to the login-list having `overflow`
        // CSS property set. Explicitly forward focus here until
        // this keyboard trap is fixed.
        if (event.shiftKey) {
          return;
        }
        let loginItem = document.querySelector("login-item");
        if (loginItem) {
          event.preventDefault();
          loginItem.shadowRoot.querySelector(".edit-button").focus();
        }
        return;
      }
      case " ":
      case "Enter": {
        event.preventDefault();
        activeDescendant.click();
        return;
      }
      default:
        return;
    }
    event.preventDefault();
    this._list.setAttribute("aria-activedescendant", newlyFocusedItem.id);
    activeDescendant.classList.remove("keyboard-selected");
    newlyFocusedItem.classList.add("keyboard-selected");
    newlyFocusedItem.scrollIntoView(false);
  }
}
customElements.define("login-list", LoginList);
