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
    let template = document.querySelector("#login-list-item-template");
    let fragment = template.content.cloneNode(true);
    let listItem = fragment.firstElementChild;

    LoginListItemFactory.update(listItem, login);

    return listItem;
  }

  static update(listItem, login) {
    let title = listItem.querySelector(".title");
    let username = listItem.querySelector(".username");
    let favicon = listItem.querySelector(".favicon");
    let faviconWrapper = listItem.querySelector(".favicon-wrapper");
    let alertIcon = listItem.querySelector(".alert-icon");

    if (!login.guid) {
      listItem.id = "new-login-list-item";
      document.l10n.setAttributes(title, "login-list-item-title-new-login");
      document.l10n.setAttributes(
        username,
        "login-list-item-subtitle-new-login"
      );
      return;
    }

    // Prepend the ID with a string since IDs must not begin with a number.
    if (!listItem.id) {
      listItem.id = "lli-" + login.guid;
      listItem.dataset.guid = login.guid;
    }
    if (title.textContent != login.title) {
      title.textContent = login.title;
    }

    let trimmedUsernameValue = login.username.trim();
    if (trimmedUsernameValue) {
      if (username.textContent != trimmedUsernameValue) {
        username.removeAttribute("data-l10n-id");
        username.textContent = trimmedUsernameValue;
      }
    } else {
      document.l10n.setAttributes(
        username,
        "login-list-item-subtitle-missing-username"
      );
    }

    if (login.faviconDataURI) {
      faviconWrapper.classList.add("hide-default-favicon");
      favicon.src = login.faviconDataURI;
    }

    if (listItem.classList.contains("breached")) {
      alertIcon.src = "chrome://global/skin/icons/warning.svg";
      document.l10n.setAttributes(
        alertIcon,
        "about-logins-list-item-breach-icon"
      );
    } else if (listItem.classList.contains("vulnerable")) {
      alertIcon.src =
        "chrome://browser/content/aboutlogins/icons/vulnerable-password.svg";

      document.l10n.setAttributes(
        alertIcon,
        "about-logins-list-item-vulnerable-password-icon"
      );
    } else {
      alertIcon.src = "";
    }
  }
}
