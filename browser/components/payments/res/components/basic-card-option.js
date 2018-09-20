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
      this[`_${name}`] = document.createElement(name == "cc-type" ? "img" : "span");
      this[`_${name}`].classList.add(name);
    }
  }

  connectedCallback() {
    for (let name of ["cc-name", "cc-number", "cc-exp", "cc-type"]) {
      this.appendChild(this[`_${name}`]);
    }
    super.connectedCallback();
  }

  static formatCCNumber(ccNumber) {
    // XXX: Bug 1470175 - This should probably be unified with CreditCard.jsm logic.
    return ccNumber ? ccNumber.replace(/[*]{4,}/, "****") : "";
  }

  static formatSingleLineLabel(basicCard) {
    let ccNumber = BasicCardOption.formatCCNumber(basicCard["cc-number"]);

    // XXX Bug 1473772 - Hard-coded string
    let ccExp = basicCard["cc-exp"] ? "Exp. " + basicCard["cc-exp"] : "";
    let ccName = basicCard["cc-name"];
    // XXX: Bug 1491040, displaying cc-type in this context may need its own localized string
    let ccType = basicCard["cc-type"] || "";
    // Filter out empty/undefined tokens before joining by three spaces
    // (&nbsp; in the middle of two normal spaces to avoid them visually collapsing in HTML)
    return [
      ccType.replace(/^[a-z]/, $0 => $0.toUpperCase()),
      ccNumber,
      ccExp,
      ccName,
      // XXX Bug 1473772 - Hard-coded string:
    ].filter(str => !!str).join(" \xa0 ");
  }

  get requiredFields() {
    return BasicCardOption.recordAttributes;
  }

  render() {
    this["_cc-name"].textContent = this.ccName || "";
    this["_cc-number"].textContent = BasicCardOption.formatCCNumber(this.ccNumber);
    // XXX Bug 1473772 - Hard-coded string:
    this["_cc-exp"].textContent = this.ccExp ? "Exp. " + this.ccExp : "";
    // XXX: Bug 1491040, displaying cc-type in this context may need its own localized string
    this["_cc-type"].alt = this.ccType || "";
    this["_cc-type"].src = "chrome://formautofill/content/icon-credit-card-generic.svg";
  }
}

customElements.define("basic-card-option", BasicCardOption);
