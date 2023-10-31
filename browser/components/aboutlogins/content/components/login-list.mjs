/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import LoginListItemFactory from "./login-list-item.mjs";
import LoginListSectionFactory from "./login-list-section.mjs";
import { recordTelemetryEvent } from "../aboutLoginsUtils.mjs";

const collator = new Intl.Collator();
const monthFormatter = new Intl.DateTimeFormat(undefined, { month: "long" });
const yearMonthFormatter = new Intl.DateTimeFormat(undefined, {
  year: "numeric",
  month: "long",
});
const dayDuration = 24 * 60 * 60_000;
const sortFnOptions = {
  name: (a, b) => collator.compare(a.title, b.title),
  "name-reverse": (a, b) => collator.compare(b.title, a.title),
  username: (a, b) => collator.compare(a.username, b.username),
  "username-reverse": (a, b) => collator.compare(b.username, a.username),
  "last-used": (a, b) => a.timeLastUsed < b.timeLastUsed,
  "last-changed": (a, b) => a.timePasswordChanged < b.timePasswordChanged,
  alerts: (a, b, breachesByLoginGUID, vulnerableLoginsByLoginGUID) => {
    const aIsBreached = breachesByLoginGUID && breachesByLoginGUID.has(a.guid);
    const bIsBreached = breachesByLoginGUID && breachesByLoginGUID.has(b.guid);
    const aIsVulnerable =
      vulnerableLoginsByLoginGUID && vulnerableLoginsByLoginGUID.has(a.guid);
    const bIsVulnerable =
      vulnerableLoginsByLoginGUID && vulnerableLoginsByLoginGUID.has(b.guid);

    if ((aIsBreached && !bIsBreached) || (aIsVulnerable && !bIsVulnerable)) {
      return -1;
    }

    if ((!aIsBreached && bIsBreached) || (!aIsVulnerable && bIsVulnerable)) {
      return 1;
    }
    return sortFnOptions.name(a, b);
  },
};

const headersFnOptions = {
  // TODO: name should use the ICU API, see Bug 1592834
  // name: l =>
  //   l.title.length && letterRegExp.test(l.title[0])
  //     ? l.title[0].toUpperCase()
  //     : "#",
  // "name-reverse": l => headersFnOptions.name(l),
  name: () => "",
  "name-reverse": () => "",
  username: () => "",
  "username-reverse": () => "",
  "last-used": l => headerFromDate(l.timeLastUsed),
  "last-changed": l => headerFromDate(l.timePasswordChanged),
  alerts: (l, breachesByLoginGUID, vulnerableLoginsByLoginGUID) => {
    const isBreached = breachesByLoginGUID && breachesByLoginGUID.has(l.guid);
    const isVulnerable =
      vulnerableLoginsByLoginGUID && vulnerableLoginsByLoginGUID.has(l.guid);
    if (isBreached) {
      return (
        LoginListSectionFactory.ID_PREFIX + "about-logins-list-section-breach"
      );
    } else if (isVulnerable) {
      return (
        LoginListSectionFactory.ID_PREFIX +
        "about-logins-list-section-vulnerable"
      );
    }
    return (
      LoginListSectionFactory.ID_PREFIX + "about-logins-list-section-nothing"
    );
  },
};

function headerFromDate(timestamp) {
  let now = new Date();
  now.setHours(0, 0, 0, 0); // reset to start of day
  let date = new Date(timestamp);

  if (now < date) {
    return (
      LoginListSectionFactory.ID_PREFIX + "about-logins-list-section-today"
    );
  } else if (now - dayDuration < date) {
    return (
      LoginListSectionFactory.ID_PREFIX + "about-logins-list-section-yesterday"
    );
  } else if (now - 7 * dayDuration < date) {
    return LoginListSectionFactory.ID_PREFIX + "about-logins-list-section-week";
  } else if (now.getFullYear() == date.getFullYear()) {
    return monthFormatter.format(date);
  } else if (now.getFullYear() - 1 == date.getFullYear()) {
    return yearMonthFormatter.format(date);
  }
  return String(date.getFullYear());
}

export default class LoginList extends HTMLElement {
  // An array of login GUIDs, stored in sorted order.
  _loginGuidsSortedOrder = [];
  // A map of login GUID -> {login, listItem}.
  _logins = {};
  // A map of section header -> sectionItem
  _sections = {};
  _filter = "";
  _selectedGuid = null;
  _blankLoginListItem = LoginListItemFactory.create({});

  constructor() {
    super();
    this._blankLoginListItem.hidden = true;
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

    // TODO: Using the addEventListener to listen for clicks and pass the event handler due to a CSP error.
    // This will be fixed as login-list itself is converted into a lit component. We will then be able to use the onclick
    // prop of login-command-button as seen in the example below (functionality works and passes tests).
    // this._createLoginButton.onClick = e => this.handleCreateNewLogin(e);

    this._createLoginButton.addEventListener("click", e =>
      this.handleCreateNewLogin(e)
    );
  }

  get #activeDescendant() {
    const activeDescendantId = this._list.getAttribute("aria-activedescendant");
    let activeDescendant =
      activeDescendantId && this.shadowRoot.getElementById(activeDescendantId);
    return activeDescendant;
  }

  selectLoginByDomainOrGuid(searchParam) {
    this._preselectLogin = searchParam;
  }

  render() {
    let visibleLoginGuids = this._applyFilter();
    this.#updateVisibleLoginCount(
      visibleLoginGuids.size,
      this._loginGuidsSortedOrder.length
    );
    this.classList.toggle("empty-search", !visibleLoginGuids.size);
    document.documentElement.classList.toggle(
      "empty-search",
      this._filter && !visibleLoginGuids.size
    );
    this._sortSelect.disabled = !visibleLoginGuids.size;

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

      if (
        !!this._breachesByLoginGUID &&
        this._breachesByLoginGUID.has(listItem.dataset.guid)
      ) {
        listItem.notificationIcon = "breached";
      } else if (
        !!this._vulnerableLoginsByLoginGUID &&
        this._vulnerableLoginsByLoginGUID.has(listItem.dataset.guid) &&
        listItem.notificationIcon !== "breached"
      ) {
        listItem.notificationIcon = "vulnerable";
      } else {
        listItem.notificationIcon = "";
      }
      listItem.hidden = !visibleLoginGuids.has(listItem.dataset.guid);
    }

    let sectionsKey = Object.keys(this._sections);
    for (let sectionKey of sectionsKey) {
      this._sections[sectionKey]._inUse = false;
    }

    if (this._loginGuidsSortedOrder.length) {
      let section = null;
      let currentHeader = null;
      // Re-arrange the login-list-items according to their sort and
      // create / re-arrange sections
      for (let i = this._loginGuidsSortedOrder.length - 1; i >= 0; i--) {
        let guid = this._loginGuidsSortedOrder[i];
        let { listItem, _header } = this._logins[guid];

        if (!listItem.hidden) {
          if (currentHeader != _header) {
            section = this.renderSectionHeader((currentHeader = _header));
          }

          section.insertBefore(
            listItem,
            section.firstElementChild.nextElementSibling
          );
        }
      }
    }

    for (let sectionKey of sectionsKey) {
      let section = this._sections[sectionKey];
      if (section._inUse) {
        continue;
      }

      section.hidden = true;
    }

    let activeDescendant = this.#activeDescendant;
    if (!activeDescendant || activeDescendant.hidden) {
      let visibleListItem = this._list.querySelector(
        "login-list-item:not([hidden])"
      );
      if (visibleListItem) {
        this._list.setAttribute("aria-activedescendant", visibleListItem.id);
      }
    }

    if (
      this._sortSelect.namedItem("alerts").hidden &&
      ((this._breachesByLoginGUID &&
        this._loginGuidsSortedOrder.some(loginGuid =>
          this._breachesByLoginGUID.has(loginGuid)
        )) ||
        (this._vulnerableLoginsByLoginGUID &&
          this._loginGuidsSortedOrder.some(loginGuid =>
            this._vulnerableLoginsByLoginGUID.has(loginGuid)
          )))
    ) {
      // Make available the "alerts" option but don't change the
      // selected sort so the user's current task isn't interrupted.
      this._sortSelect.namedItem("alerts").hidden = false;
    }
  }

  renderSectionHeader(header) {
    let section = this._sections[header];
    if (!section) {
      section = this._sections[header] = LoginListSectionFactory.create(header);
    }

    this._list.insertBefore(
      section,
      this._blankLoginListItem.nextElementSibling
    );

    section._inUse = true;
    section.hidden = false;
    return section;
  }

  handleCreateNewLogin() {
    window.dispatchEvent(
      new CustomEvent("AboutLoginsShowBlankLogin", {
        cancelable: true,
      })
    );
    recordTelemetryEvent({ object: "new_login", method: "new" });
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        let listItem;
        if (event.originalTarget.tagName === "LOGIN-LIST-ITEM") {
          listItem = event.originalTarget;
        } else {
          listItem = event.originalTarget
            ? event.originalTarget.getRootNode().host
            : null;
        }
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

        let extra = {};
        if (listItem.notificationIcon === "breached") {
          extra = { breached: "true" };
        } else if (listItem.notificationIcon === "vulnerable") {
          extra = { vulnerable: "true" };
        }
        recordTelemetryEvent({
          object: "existing_login",
          method: "select",
          extra,
        });
        break;
      }
      case "change": {
        this._applyHeaders();
        this._applySortAndScrollToTop();
        const extra = { sort_key: this._sortSelect.value };
        recordTelemetryEvent({ object: "list", method: "sort", extra });
        document.dispatchEvent(
          new CustomEvent("AboutLoginsSortChanged", {
            bubbles: true,
            detail: this._sortSelect.value,
          })
        );
        break;
      }
      case "AboutLoginsClearSelection": {
        if (!this._loginGuidsSortedOrder.length) {
          this._createLoginButton.disabled = false;
          this.classList.remove("create-login-selected");
          return;
        }

        let firstVisibleListItem = this._list.querySelector(
          "login-list-item[data-guid]:not([hidden])"
        );
        let newlySelectedLogin;
        if (firstVisibleListItem) {
          newlySelectedLogin =
            this._logins[firstVisibleListItem.dataset.guid].login;
        } else {
          // Clear the filter if all items have been filtered out.
          this.classList.remove("create-login-selected");
          this._createLoginButton.disabled = false;
          window.dispatchEvent(
            new CustomEvent("AboutLoginsFilterLogins", {
              detail: "",
            })
          );
          newlySelectedLogin =
            this._logins[this._loginGuidsSortedOrder[0]].login;
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

        // XXX If an AboutLoginsLoginSelected event is received that doesn't contain
        // the full login object, re-dispatch the event with the full login object since
        // only the login-list knows the full details of each login object.
        if (
          Object.keys(event.detail).length == 1 &&
          event.detail.hasOwnProperty("guid")
        ) {
          window.dispatchEvent(
            new CustomEvent("AboutLoginsLoginSelected", {
              detail: this._logins[event.detail.guid].login,
              cancelable: true,
            })
          );
          return;
        }

        let listItem = this._list.querySelector(
          `login-list-item[data-guid="${event.detail.guid}"]`
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
      case "keyup":
      case "keydown": {
        if (event.type == "keydown") {
          if (
            this.shadowRoot.activeElement &&
            this.shadowRoot.activeElement.closest("ol") &&
            (event.key == " " ||
              event.key == "ArrowUp" ||
              event.key == "ArrowDown")
          ) {
            // Since Space, ArrowUp and ArrowDown will perform actions, prevent
            // them from also scrolling the list.
            event.preventDefault();
          }
        }

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
    this._sections = {};
    this._applyHeaders();
    this._applySort();
    this._list.textContent = "";
    this._list.appendChild(this._blankLoginListItem);
    this.render();

    if (!this._selectedGuid || !this._logins[this._selectedGuid]) {
      this._selectFirstVisibleLogin();
    }
  }

  /**
   * @param {Map} breachesByLoginGUID A Map of breaches by login GUIDs used
   *                                  for displaying breached login indicators.
   */
  setBreaches(breachesByLoginGUID) {
    this._internalSetMonitorData("_breachesByLoginGUID", breachesByLoginGUID);
  }

  /**
   * @param {Map} breachesByLoginGUID A Map of breaches by login GUIDs that
   *                                  should be added to the local cache of
   *                                  breaches.
   */
  updateBreaches(breachesByLoginGUID) {
    this._internalUpdateMonitorData(
      "_breachesByLoginGUID",
      breachesByLoginGUID
    );
  }

  setVulnerableLogins(vulnerableLoginsByLoginGUID) {
    this._internalSetMonitorData(
      "_vulnerableLoginsByLoginGUID",
      vulnerableLoginsByLoginGUID
    );
  }

  updateVulnerableLogins(vulnerableLoginsByLoginGUID) {
    this._internalUpdateMonitorData(
      "_vulnerableLoginsByLoginGUID",
      vulnerableLoginsByLoginGUID
    );
  }

  _internalSetMonitorData(
    internalMemberName,
    mapByLoginGUID,
    updateSortAndSelectedLogin = true
  ) {
    this[internalMemberName] = mapByLoginGUID;
    if (this[internalMemberName].size) {
      for (let [loginGuid] of mapByLoginGUID) {
        if (this._logins[loginGuid]) {
          let { login, listItem } = this._logins[loginGuid];
          LoginListItemFactory.update(listItem, login);
        }
      }
      if (updateSortAndSelectedLogin) {
        const alertsSortOptionElement = this._sortSelect.namedItem("alerts");
        alertsSortOptionElement.hidden = false;
        this._sortSelect.selectedIndex = alertsSortOptionElement.index;
        this._applyHeaders();
        this._applySortAndScrollToTop();
        this._selectFirstVisibleLogin();
      }
    }
    this.render();
  }

  _internalUpdateMonitorData(internalMemberName, mapByLoginGUID) {
    if (!this[internalMemberName]) {
      this[internalMemberName] = new Map();
    }
    for (const [guid, data] of [...mapByLoginGUID]) {
      if (data) {
        this[internalMemberName].set(guid, data);
      } else {
        this[internalMemberName].delete(guid);
      }
    }
    this._internalSetMonitorData(
      internalMemberName,
      this[internalMemberName],
      false
    );
  }

  setSortDirection(sortDirection) {
    // The 'alerts' sort becomes visible when there are known alerts.
    // Don't restore to the 'alerts' sort if there are no alerts to show.
    if (
      sortDirection == "alerts" &&
      this._sortSelect.namedItem("alerts").hidden
    ) {
      return;
    }
    this._sortSelect.value = sortDirection;
    this._applyHeaders();
    this._applySortAndScrollToTop();
    this._selectFirstVisibleLogin();
  }

  /**
   * @param {login} login A login that was added to storage.
   */
  loginAdded(login) {
    this._logins[login.guid] = { login };
    this._loginGuidsSortedOrder.push(login.guid);
    this._applyHeaders(false);
    this._applySort();

    // Add the list item and update any other related state that may pertain
    // to the list item such as breach alerts.
    this.render();

    if (
      this.classList.contains("no-logins") &&
      !this.classList.contains("create-login-selected")
    ) {
      this._selectFirstVisibleLogin();
    }
  }

  /**
   * @param {login} login A login that was modified in storage. The related
   *                      login-list-item will get updated.
   */
  loginModified(login) {
    this._logins[login.guid] = Object.assign(this._logins[login.guid], {
      login,
      _header: null, // reset header
    });
    this._applyHeaders(false);
    this._applySort();
    let loginObject = this._logins[login.guid];
    LoginListItemFactory.update(loginObject.listItem, login);

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
        "login-list-item[data-guid]:not([hidden])"
      );
      if (visibleListItems.length > 1) {
        let index = [...visibleListItems].findIndex(listItem => {
          return listItem.dataset.guid == login.guid;
        });
        let newlySelectedIndex = index > 0 ? index - 1 : index + 1;
        let newlySelectedLogin =
          this._logins[visibleListItems[newlySelectedIndex].dataset.guid].login;
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
            login.password.toLocaleLowerCase().includes(this._filter)
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
      return sortFnOptions[sort](
        loginA,
        loginB,
        this._breachesByLoginGUID,
        this._vulnerableLoginsByLoginGUID
      );
    });
  }

  _applyHeaders(updateAll = true) {
    let headerFn = headersFnOptions[this._sortSelect.value];
    for (let guid of this._loginGuidsSortedOrder) {
      let login = this._logins[guid];
      if (updateAll || !login._header) {
        login._header = headerFn(
          login.login,
          this._breachesByLoginGUID,
          this._vulnerableLoginsByLoginGUID
        );
      }
    }
  }

  _applySortAndScrollToTop() {
    this._applySort();
    this.render();
    this._list.scrollTop = 0;
  }

  #updateVisibleLoginCount(count, total) {
    const args = document.l10n.getAttributes(this._count).args;
    if (count != args.count || total != args.total) {
      document.l10n.setAttributes(
        this._count,
        count == total ? "login-list-count" : "login-list-filtered-count",
        { count, total }
      );
    }
  }

  #findPreviousItem(item) {
    let previousItem = item;
    do {
      previousItem =
        (previousItem.tagName == "SECTION"
          ? previousItem.lastElementChild
          : previousItem.previousElementSibling) ||
        (previousItem.parentElement.tagName == "SECTION" &&
          previousItem.parentElement.previousElementSibling);
    } while (
      previousItem &&
      (previousItem.hidden || previousItem.tagName !== "LOGIN-LIST-ITEM")
    );

    return previousItem;
  }

  #findNextItem(item) {
    let nextItem = item;
    do {
      nextItem =
        (nextItem.tagName == "SECTION"
          ? nextItem.firstElementChild.nextElementSibling
          : nextItem.nextElementSibling) ||
        (nextItem.parentElement.tagName == "SECTION" &&
          nextItem.parentElement.nextElementSibling);
    } while (
      nextItem &&
      (nextItem.hidden || nextItem.tagName !== "LOGIN-LIST-ITEM")
    );
    return nextItem;
  }

  #pickByDirection(ltr, rtl) {
    return document.dir == "ltr" ? ltr : rtl;
  }

  //TODO May be we can use this fn in render(), but logic is different a bit
  get #activeDescendantForSelection() {
    let activeDescendant = this.#activeDescendant;
    if (
      !activeDescendant ||
      activeDescendant.hidden ||
      activeDescendant.tagName !== "LOGIN-LIST-ITEM"
    ) {
      activeDescendant =
        this._list.querySelector("login-list-item[data-guid]:not([hidden])") ||
        this._list.firstElementChild;
    }
    return activeDescendant;
  }

  _handleKeyboardNavWithinList(event) {
    if (this._list != this.shadowRoot.activeElement) {
      return;
    }

    let command = null;

    switch (event.type) {
      case "keyup":
        switch (event.key) {
          case " ":
          case "Enter":
            command = "click";
            break;
        }
        break;
      case "keydown":
        switch (event.key) {
          case "ArrowDown":
            command = "next";
            break;
          case "ArrowLeft":
            command = this.#pickByDirection("previous", "next");
            break;
          case "ArrowRight":
            command = this.#pickByDirection("next", "previous");
            break;
          case "ArrowUp":
            command = "previous";
            break;
        }
        break;
    }

    if (command) {
      event.preventDefault();

      switch (command) {
        case "click":
          this.clickSelected();
          break;
        case "next":
          this.selectNext();
          break;
        case "previous":
          this.selectPrevious();
          break;
      }
    }
  }

  clickSelected() {
    this.#activeDescendantForSelection?.click();
  }

  selectNext() {
    const activeDescendant = this.#activeDescendantForSelection;
    if (activeDescendant) {
      this.#moveSelection(
        activeDescendant,
        this.#findNextItem(activeDescendant)
      );
    }
  }

  selectPrevious() {
    const activeDescendant = this.#activeDescendantForSelection;
    if (activeDescendant) {
      this.#moveSelection(
        activeDescendant,
        this.#findPreviousItem(activeDescendant)
      );
    }
  }

  #moveSelection(from, to) {
    if (to) {
      this._list.setAttribute("aria-activedescendant", to.id);
      from?.classList.remove("keyboard-selected");
      to.classList.add("keyboard-selected");
      to.scrollIntoView({ block: "nearest" });
      this.clickSelected();
    }
  }

  /**
   * Selects the first visible login as part of the initial load of the page,
   * which will bypass any focus changes that occur during manual login
   * selection.
   */
  _selectFirstVisibleLogin() {
    const visibleLoginsGuids = this._applyFilter();
    let selectedLoginGuid =
      this._loginGuidsSortedOrder.find(guid => guid === this._preselectLogin) ??
      this.findLoginGuidFromDomain(this._preselectLogin) ??
      this._loginGuidsSortedOrder[0];

    selectedLoginGuid = [
      selectedLoginGuid,
      ...this._loginGuidsSortedOrder,
    ].find(guid => visibleLoginsGuids.has(guid));

    const selectedLogin = this._logins[selectedLoginGuid]?.login;

    if (selectedLogin) {
      window.dispatchEvent(
        new CustomEvent("AboutLoginsInitialLoginSelected", {
          detail: selectedLogin,
        })
      );
      this.updateSelectedLocationHash(selectedLoginGuid);
    }
  }

  _setListItemAsSelected(listItem) {
    let oldSelectedItem = this._list.querySelector(".selected");
    if (oldSelectedItem) {
      oldSelectedItem.classList.remove("selected");
      oldSelectedItem.selected = false;
      oldSelectedItem.removeAttribute("aria-selected");
    }
    this.classList.toggle("create-login-selected", !listItem.dataset.guid);
    this._blankLoginListItem.hidden = !!listItem.dataset.guid;
    this._createLoginButton.disabled = !listItem.dataset.guid;
    listItem.classList.add("selected");
    listItem.selected = true;
    listItem.setAttribute("aria-selected", "true");
    this._list.setAttribute("aria-activedescendant", listItem.id);
    this._selectedGuid = listItem.dataset.guid;
    this.updateSelectedLocationHash(this._selectedGuid);
    // Scroll item into view if it isn't visible
    listItem.scrollIntoView({ block: "nearest" });
  }

  updateSelectedLocationHash(guid) {
    window.location.hash = guid ? `#${encodeURIComponent(guid)}` : "";
  }

  findLoginGuidFromDomain(domain) {
    for (let guid of this._loginGuidsSortedOrder) {
      let login = this._logins[guid].login;
      if (login.hostname === domain) {
        return guid;
      }
    }
    return null;
  }
}
customElements.define("login-list", LoginList);
