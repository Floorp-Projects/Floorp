/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";
import RichOption from "./rich-option.js";

/**
 * <rich-select>
 *  <basic-card-option></basic-card-option>
 * </rich-select>
 */

export default class BasicCardOption extends ObservedPropertiesMixin(RichOption) {
  static get recordAttributes() {
    return [
      "cc-exp",
      "cc-name",
      "cc-number",
      "cc-type",
      "guid",
    ];
  }

  static get observedAttributes() {
    return RichOption.observedAttributes.concat(BasicCardOption.recordAttributes);
  }

  constructor() {
    super();

    for (let name of ["cc-name", "cc-number", "cc-exp", "cc-type"]) {
      this[`_${name}`] = document.createElement("span");
      this[`_${name}`].classList.add(name);
    }
  }

  connectedCallback() {
    for (let name of ["cc-name", "cc-number", "cc-exp", "cc-type"]) {
      this.appendChild(this[`_${name}`]);
    }
    super.connectedCallback();
  }

  static formatSingleLineLabel(basicCard) {
    // Fall back to empty strings to prevent 'undefined' from appearing.
    let ccNumber = basicCard["cc-number"] || "";
    let ccExp = basicCard["cc-exp"] || "";
    let ccName = basicCard["cc-name"] || "";
    // XXX: Bug 1491040, displaying cc-type in this context may need its own localized string
    let ccType = basicCard["cc-type"] || "";
    return `${ccType} ${ccNumber} ${ccExp} ${ccName}`;
  }

  render() {
    this["_cc-name"].textContent = this.ccName;
    this["_cc-number"].textContent = this.ccNumber;
    this["_cc-exp"].textContent = this.ccExp;
    this["_cc-type"].textContent = this.ccType;
  }
}

customElements.define("basic-card-option", BasicCardOption);
