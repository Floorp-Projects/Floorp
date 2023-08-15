/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LoginListItem, NewListItem } from "./login-list-lit-item.mjs";
/**
 * This file will be removed once login-list is made into a lit-component since
 * whatever the update function does, lit does internally automatically.
 */
export default class LoginListItemFactory {
  static create(login) {
    if (!login.guid) {
      const newListItem = new NewListItem();
      newListItem.classList.add("list-item");
      return newListItem;
    }
    const loginListItem = new LoginListItem();
    loginListItem.classList.add("list-item");
    LoginListItemFactory.update(loginListItem, login);
    return loginListItem;
  }

  static update(listItem, login) {
    listItem.title = login.title;
    listItem.username = login.username;
    listItem.favicon = `page-icon:${login.origin}`;

    // Prepend the ID with a string since IDs must not begin with a number.
    if (!listItem.id) {
      listItem.id = "lli-" + login.guid;
      listItem.dataset.guid = login.guid;
    }
  }
}
