/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LoginListItemFactory is used instead of the "login-list-item" custom element
 * since there may be 100s of login-list-items on about:logins and each
 * custom element carries with it significant overhead when used in large
 * numbers.
 */
export default class LoginListItemFactory {
  static create(login) {
    let loginListItemTemplate = document.querySelector(
      "#login-list-item-template"
    );
    let loginListItem = loginListItemTemplate.content.cloneNode(true);
    let listItem = loginListItem.querySelector("li");
    let title = loginListItem.querySelector(".title");
    let username = loginListItem.querySelector(".username");

    listItem.setAttribute("role", "option");

    if (!login.guid) {
      listItem.id = "new-login-list-item";
      document.l10n.setAttributes(title, "login-list-item-title-new-login");
      document.l10n.setAttributes(
        username,
        "login-list-item-subtitle-new-login"
      );
      return listItem;
    }

    // Prepend the ID with a string since IDs must not begin with a number.
    listItem.id = "lli-" + login.guid;
    listItem.dataset.guid = login.guid;
    listItem._login = login;
    title.textContent = login.title;
    if (login.username.trim()) {
      username.removeAttribute("data-l10n-id");
      username.textContent = login.username.trim();
    } else {
      document.l10n.setAttributes(
        username,
        "login-list-item-subtitle-missing-username"
      );
    }

    return listItem;
  }
}
