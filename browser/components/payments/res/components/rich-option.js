/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * <rich-select>
 *  <rich-option></rich-option>
 * </rich-select>
 */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";

export default class RichOption extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "selected",
      "value",
    ];
  }

  connectedCallback() {
    this.classList.add("rich-option");
    this.render();
  }

  render() {}
}

customElements.define("rich-option", RichOption);
