/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import LoginListItemFactory from "./login-list-item.js";
import { recordTelemetryEvent } from "../aboutLoginsUtils.js";

const collator = new Intl.Collator();
const sortFnOptions = {
  name: (a, b) => collator.compare(a.title, b.title),
  "last-used": (a, b) => a.timeLastUsed < b.timeLastUsed,
  "last-changed": (a, b) => a.timePasswordChanged < b.timePasswordChanged,
  breached: (a, b, breachesByLoginGUID) => {
    if (!breachesByLoginGUID) {
      return 0;
    }
    const aIsBreached = breachesByLoginGUID.has(a.guid);
    const bIsBreached = breachesByLoginGUID.has(b.guid);
    if (aIsBreached && !bIsBreached) {
      return -1;
    }
    if (!aIsBreached && bIsBreached) {
      return 1;
    }
    return collator.compare(a.title, b.title);
  },
};

export default class LoginList extends HTMLElement {
  constructor() {
    super();
    // An array of login GUIDs, stored in sorted order.
    this._loginGuidsSortedOrder = [];
    // A map of login GUID -> {login, listItem}.
    this._logins = {};
    this._filter = "";
    this._selectedGuid = null;
    this._blankLoginListItem = LoginListItemFactory.create({});
  }

  connectedCallback() {
    if (this.shadowRoot) {
      return;
    }
    let loginListTemplate = document.querySelector("#login-list-template");
    let shadowRoot = this.attachShadow({ mode: "open" });
    document.l10n.connectRoot(shadowRoot);
    shadowRoot.appendChild(loginListTemplate.content.cloneNode(true));

    this._count = shadowRoot.querySelector(".count");
    this._createLoginButton = shadowRoot.querySelector(".create-login-button");
    this._list = shadowRoot.querySelector("ol");
    this._list.appendChild(this._blankLoginListItem);
    this._sortSelect = shadowRoot.querySelector("#login-sort");

    this.render();

    this.shadowRoot
      .getElementById("login-sort")
      .addEventListener("change", this);
    window.addEventListener("AboutLoginsClearSelection", this);
    window.addEventListener("AboutLoginsFilterLogins", this);
    window.addEventListener("AboutLoginsInitialLoginSelected", this);
    window.addEventListener("AboutLoginsLoginSelected", this);
    window.addEventListener("AboutLoginsShowBlankLogin", this);
    this._list.addEventListener("click", this);
    this.addEventListener("keydown", this);
    this.addEventListener("keyup", this);
    this._createLoginButton.addEventListener("click", this);
  }

  render() {
    let visibleLoginGuids = this._applyFilter();
    this._updateVisibleLoginCount(visibleLoginGuids.size);
    this.classList.toggle("empty-search", !visibleLoginGuids.size);
    document.documentElement.classList.toggle(
      "empty-search",
      this._filter && !visibleLoginGuids.size
    );

    // Add all of the logins that are not in the DOM yet.
    let fragment = document.createDocumentFragment();
    for (let guid of this._loginGuidsSortedOrder) {
      if (this._logins[guid].listItem) {
        continue;
      }
      let login = this._logins[guid].login;
      let listItem = LoginListItemFactory.create(login);
      this._logins[login.guid] = Object.assign(this._logins[login.guid], {
        listItem,
      });
      fragment.appendChild(listItem);
    }
    this._list.appendChild(fragment);

    // Show, hide, and update state of the list items per the applied search filter.
    for (let guid of this._loginGuidsSortedOrder) {
      let { listItem } = this._logins[guid];

      if (guid == this._selectedGuid) {
        this._setListItemAsSelected(listItem);
      }
      listItem.classList.toggle(
        "breached",
        !!this._breachesByLoginGUID &&
          this._breachesByLoginGUID.has(listItem.dataset.guid)
      );
      listItem.hidden = !visibleLoginGuids.has(listItem.dataset.guid);
    }

    // Re-arrange the login-list-items according to their sort
    for (let i = this._loginGuidsSortedOrder.length - 1; i >= 0; i--) {
      let guid = this._loginGuidsSortedOrder[i];
      let { listItem } = this._logins[guid];
      this._list.insertBefore(
        listItem,
        this._blankLoginListItem.nextElementSibling
      );
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        if (event.originalTarget == this._createLoginButton) {
          window.dispatchEvent(
            new CustomEvent("AboutLoginsShowBlankLogin", {
              cancelable: true,
            })
          );
          recordTelemetryEvent({ object: "new_login", method: "new" });
          return;
        }

        let listItem = event.originalTarget.closest(".login-list-item");
        if (!listItem || !listItem.dataset.guid) {
          return;
        }

        let { login } = this._logins[listItem.dataset.guid];
        this.dispatchEvent(
          new CustomEvent("AboutLoginsLoginSelected", {
            bubbles: true,
            composed: true,
            cancelable: true, // allow calling preventDefault() on event
            detail: login,
          })
        );

        const extra = listItem.classList.contains("breached")
          ? { breached: "true" }
          : {};
        recordTelemetryEvent({
          object: "existing_login",
          method: "select",
          extra,
        });
        break;
      }
      case "change": {
        this._applySort();
        this.render();
        const extra = { sort_key: this._sortSelect.value };
        recordTelemetryEvent({ object: "list", method: "sort", extra });
        break;
      }
      case "AboutLoginsClearSelection": {
        if (!this._loginGuidsSortedOrder.length) {
          return;
        }

        let firstVisibleListItem = this._list.querySelector(
          ".login-list-item[data-guid]:not([hidden])"
        );
        let newlySelectedLogin;
        if (firstVisibleListItem) {
          newlySelectedLogin = this._logins[firstVisibleListItem.dataset.guid]
            .login;
        } else {
          // Clear the filter if all items have been filtered out.
          this.classList.remove("create-login-selected");
          this._createLoginButton.disabled = false;
          window.dispatchEvent(
            new CustomEvent("AboutLoginsFilterLogins", {
              detail: "",
            })
          );
          newlySelectedLogin = this._logins[this._loginGuidsSortedOrder[0]]
            .login;
        }

        // Select the first visible login after any possible filter is applied.
        window.dispatchEvent(
          new CustomEvent("AboutLoginsLoginSelected", {
            detail: newlySelectedLogin,
            cancelable: true,
          })
        );
        break;
      }
      case "AboutLoginsFilterLogins": {
        this._filter = event.detail.toLocaleLowerCase();
        this.render();
        break;
      }
      case "AboutLoginsInitialLoginSelected":
      case "AboutLoginsLoginSelected": {
        if (event.defaultPrevented || this._selectedGuid == event.detail.guid) {
          return;
        }

        let listItem = this._list.querySelector(
          `.login-list-item[data-guid="${event.detail.guid}"]`
        );
        if (listItem) {
          this._setListItemAsSelected(listItem);
        } else {
          this.render();
        }
        break;
      }
      case "AboutLoginsShowBlankLogin": {
        if (!event.defaultPrevented) {
          this._selectedGuid = null;
          this._setListItemAsSelected(this._blankLoginListItem);
        }
        break;
      }
      case "keydown": {
        this._handleTabbingToExternalElements(event);

        // Since Space will select a login in the list, prevent it from
        // also scrolling the list.
        if (
          this.shadowRoot.activeElement &&
          this.shadowRoot.activeElement.closest("ol") &&
          event.key == " "
        ) {
          event.preventDefault();
        }
        break;
      }
      case "keyup": {
        this._handleKeyboardNavWithinList(event);
        break;
      }
    }
  }

  /**
   * @param {login[]} logins An array of logins used for displaying in the list.
   */
  setLogins(logins) {
    this._loginGuidsSortedOrder = [];
    this._logins = logins.reduce((map, login) => {
      this._loginGuidsSortedOrder.push(login.guid);
      map[login.guid] = { login };
      return map;
    }, {});
    this._applySort();
    this._list.textContent = "";
    this._list.appendChild(this._blankLoginListItem);
    this.render();

    if (!this._selectedGuid || !this._logins[this._selectedGuid]) {
      // Select the first visible login after any possible filter is applied.
      let firstVisibleListItem = this._list.querySelector(
        ".login-list-item[data-guid]:not([hidden])"
      );
      if (firstVisibleListItem) {
        let { login } = this._logins[firstVisibleListItem.dataset.guid];
        window.dispatchEvent(
          new CustomEvent("AboutLoginsInitialLoginSelected", {
            detail: login,
          })
        );
      }
    }
  }

  addFavicons(favicons) {
    for (let favicon in favicons) {
      let { login, listItem } = this._logins[favicon];
      favicon = favicons[favicon];
      if (favicon && login.title) {
        favicon.uri = btoa(
          new Uint8Array(favicon.data).reduce(
            (data, byte) => data + String.fromCharCode(byte),
            ""
          )
        );
        let faviconDataURI = `data:${favicon.mimeType};base64,${favicon.uri}`;
        login.faviconDataURI = faviconDataURI;
      }
      LoginListItemFactory.update(listItem, login);
    }
    let selectedListItem = this._list.querySelector(
      ".login-list-item.selected"
    );
    if (selectedListItem) {
      window.dispatchEvent(
        new CustomEvent("AboutLoginsLoadInitialFavicon", {})
      );
    }
    this.render();
  }

  /**
   * @param {Map} breachesByLoginGUID A Map of breaches by login GUIDs used
   *                                  for displaying breached login indicators.
   */
  updateBreaches(breachesByLoginGUID) {
    this._breachesByLoginGUID = breachesByLoginGUID;
    if (this._breachesByLoginGUID.size === 0) {
      this.render();
      return;
    }
    const breachedSortOptionElement = this._sortSelect.namedItem("breached");
    breachedSortOptionElement.hidden = false;
    this._sortSelect.selectedIndex = breachedSortOptionElement.index;
    this._sortSelect.dispatchEvent(new CustomEvent("input", { bubbles: true }));
    this._sortSelect.dispatchEvent(
      new CustomEvent("change", { bubbles: true })
    );
    this.render();
  }

  /**
   * @param {login} login A login that was added to storage.
   */
  loginAdded(login) {
    this._logins[login.guid] = { login };
    this._loginGuidsSortedOrder.push(login.guid);
    this._applySort();

    // Add the list item and update any other related state that may pertain
    // to the list item such as breach alerts.
    this.render();
  }

  /**
   * @param {login} login A login that was modified in storage. The related
   *                      login-list-item will get updated.
   */
  loginModified(login) {
    this._logins[login.guid] = Object.assign(this._logins[login.guid], {
      login,
    });
    this._applySort();
    let { listItem } = this._logins[login.guid];
    LoginListItemFactory.update(listItem, login);

    // Update any other related state that may pertain to the list item
    // such as breach alerts that may or may not now apply.
    this.render();
  }

  /**
   * @param {login} login A login that was removed from storage. The related
   *                      login-list-item will get removed. The login object
   *                      is a plain JS object representation of
   *                      nsILoginInfo/nsILoginMetaInfo.
   */
  loginRemoved(login) {
    // Update the selected list item to the previous item in the list
    // if one exists, otherwise the next item. If no logins remain
    // the login-intro or empty-search text will be shown instead of the login-list.
    if (this._selectedGuid == login.guid) {
      let visibleListItems = this._list.querySelectorAll(
        ".login-list-item[data-guid]:not([hidden])"
      );
      if (visibleListItems.length > 1) {
        let index = [...visibleListItems].findIndex(listItem => {
          return listItem.dataset.guid == login.guid;
        });
        let newlySelectedIndex = index > 0 ? index - 1 : index + 1;
        let newlySelectedLogin = this._logins[
          visibleListItems[newlySelectedIndex].dataset.guid
        ].login;
        window.dispatchEvent(
          new CustomEvent("AboutLoginsLoginSelected", {
            detail: newlySelectedLogin,
            cancelable: true,
          })
        );
      }
    }

    this._logins[login.guid].listItem.remove();
    delete this._logins[login.guid];
    this._loginGuidsSortedOrder = this._loginGuidsSortedOrder.filter(guid => {
      return guid != login.guid;
    });

    // Render the login-list to update the search result count and show the
    // empty-search message if needed.
    this.render();
  }

  /**
   * @returns {Set} Set of login guids that match the filter.
   */
  _applyFilter() {
    let matchingLoginGuids;
    if (this._filter) {
      matchingLoginGuids = new Set(
        this._loginGuidsSortedOrder.filter(guid => {
          let { login } = this._logins[guid];
          return (
            login.origin.toLocaleLowerCase().includes(this._filter) ||
            (!!login.httpRealm &&
              login.httpRealm.toLocaleLowerCase().includes(this._filter)) ||
            login.username.toLocaleLowerCase().includes(this._filter) ||
            (!window.AboutLoginsUtils.masterPasswordEnabled &&
              login.password.toLocaleLowerCase().includes(this._filter))
          );
        })
      );
    } else {
      matchingLoginGuids = new Set([...this._loginGuidsSortedOrder]);
    }

    return matchingLoginGuids;
  }

  _applySort() {
    const sort = this._sortSelect.value;
    this._loginGuidsSortedOrder = this._loginGuidsSortedOrder.sort((a, b) => {
      let loginA = this._logins[a].login;
      let loginB = this._logins[b].login;
      return sortFnOptions[sort](loginA, loginB, this._breachesByLoginGUID);
    });
  }

  _updateVisibleLoginCount(count) {
    if (count != document.l10n.getAttributes(this._count).args.count) {
      document.l10n.setAttributes(this._count, "login-list-count", {
        count,
      });
    }
  }

  _handleTabbingToExternalElements(event) {
    if (
      (this._createLoginButton == this.shadowRoot.activeElement ||
        (this._list == this.shadowRoot.activeElement &&
          this._createLoginButton.disabled)) &&
      event.key == "Tab"
    ) {
      // Bug 1562716: Pressing Tab from the create-login-button cycles back to the
      // login-sort dropdown due to the login-list having `overflow`
      // CSS property set. Explicitly forward focus here until
      // this keyboard trap is fixed.
      if (event.shiftKey) {
        return;
      }
      if (
        this.classList.contains("no-logins") &&
        !this.classList.contains("create-login-selected")
      ) {
        let loginIntro = document.querySelector("login-intro");
        event.preventDefault();
        loginIntro.focus();
        return;
      }
      let loginItem = document.querySelector("login-item");
      if (loginItem) {
        event.preventDefault();
        loginItem.focus();
      }
    }
  }

  _handleKeyboardNavWithinList(event) {
    if (this._list != this.shadowRoot.activeElement) {
      return;
    }

    let isLTR = document.dir == "ltr";
    let activeDescendantId = this._list.getAttribute("aria-activedescendant");
    let activeDescendant =
      activeDescendantId && this.shadowRoot.getElementById(activeDescendantId);
    if (!activeDescendant || activeDescendant.hidden) {
      activeDescendant =
        this._list.querySelector(".login-list-item[data-guid]:not([hidden])") ||
        this._list.firstElementChild;
    }
    let newlyFocusedItem = null;
    let previousItem = activeDescendant.previousElementSibling;
    while (previousItem && previousItem.hidden) {
      previousItem = previousItem.previousElementSibling;
    }
    let nextItem = activeDescendant.nextElementSibling;
    while (nextItem && nextItem.hidden) {
      nextItem = nextItem.nextElementSibling;
    }
    switch (event.key) {
      case "ArrowDown": {
        if (!nextItem) {
          return;
        }
        newlyFocusedItem = nextItem;
        break;
      }
      case "ArrowLeft": {
        let item = isLTR ? previousItem : nextItem;
        if (!item) {
          return;
        }
        newlyFocusedItem = item;
        break;
      }
      case "ArrowRight": {
        let item = isLTR ? nextItem : previousItem;
        if (!item) {
          return;
        }
        newlyFocusedItem = item;
        break;
      }
      case "ArrowUp": {
        if (!previousItem) {
          return;
        }
        newlyFocusedItem = previousItem;
        break;
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
    newlyFocusedItem.scrollIntoView({ block: "nearest" });
  }

  _setListItemAsSelected(listItem) {
    let oldSelectedItem = this._list.querySelector(".selected");
    if (oldSelectedItem) {
      oldSelectedItem.classList.remove("selected");
      oldSelectedItem.removeAttribute("aria-selected");
    }
    this.classList.toggle("create-login-selected", !listItem.dataset.guid);
    this._createLoginButton.disabled = !listItem.dataset.guid;
    listItem.classList.add("selected");
    listItem.setAttribute("aria-selected", "true");
    this._list.setAttribute("aria-activedescendant", listItem.id);
    this._selectedGuid = listItem.dataset.guid;

    // Scroll item into view if it isn't visible
    listItem.scrollIntoView({ block: "nearest" });
  }
}
customElements.define("login-list", LoginList);
