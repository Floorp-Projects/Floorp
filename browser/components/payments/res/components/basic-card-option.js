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
      "guid",
      "type", // XXX Bug 1429181.
    ];
  }

  static get observedAttributes() {
    return RichOption.observedAttributes.concat(BasicCardOption.recordAttributes);
  }

  constructor() {
    super();

    for (let name of ["cc-name", "cc-number", "cc-exp", "type"]) {
      this[`_${name}`] = document.createElement("span");
      this[`_${name}`].classList.add(name);
    }
  }

  connectedCallback() {
    for (let name of ["cc-name", "cc-number", "cc-exp", "type"]) {
      this.appendChild(this[`_${name}`]);
    }
    super.connectedCallback();
  }

  static formatSingleLineLabel(basicCard) {
    return basicCard["cc-number"] + " " + basicCard["cc-exp"] + " " + basicCard["cc-name"];
  }

  render() {
    this["_cc-name"].textContent = this.ccName;
    this["_cc-number"].textContent = this.ccNumber;
    this["_cc-exp"].textContent = this.ccExp;
    this._type.textContent = this.type;
  }
}

customElements.define("basic-card-option", BasicCardOption);
