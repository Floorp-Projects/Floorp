/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default class LoginListHeaderFactory {
  static ID_PREFIX = "id-";

  static create(header) {
    let template = document.querySelector("#login-list-section-template");
    let fragment = template.content.cloneNode(true);
    let sectionItem = fragment.firstElementChild;

    this.update(sectionItem, header);

    return sectionItem;
  }

  static update(headerItem, header) {
    let headerElement = headerItem.querySelector(".login-list-header");
    if (header) {
      if (header.startsWith(this.ID_PREFIX)) {
        document.l10n.setAttributes(
          headerElement,
          header.substring(this.ID_PREFIX.length)
        );
      } else {
        headerElement.textContent = header;
      }
      headerElement.hidden = false;
    } else {
      headerElement.hidden = true;
    }
  }
}
