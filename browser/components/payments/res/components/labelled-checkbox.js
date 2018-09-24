/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";

/**
 *  <labelled-checkbox label="Some label" value="The value"></labelled-checkbox>
 */

export default class LabelledCheckbox extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "form",
      "label",
      "value",
    ];
  }
  constructor() {
    super();

    this._label = document.createElement("label");
    this._labelSpan = document.createElement("span");
    this._checkbox = document.createElement("input");
    this._checkbox.type = "checkbox";
  }

  connectedCallback() {
    this.appendChild(this._label);
    this._label.appendChild(this._checkbox);
    this._label.appendChild(this._labelSpan);
    this.render();
  }

  render() {
    this._labelSpan.textContent = this.label;
    // We don't use the ObservedPropertiesMixin behaviour because we want to be able to mirror
    // form="" but ObservedPropertiesMixin removes attributes when "".
    if (this.hasAttribute("form")) {
      this._checkbox.setAttribute("form", this.getAttribute("form"));
    } else {
      this._checkbox.removeAttribute("form");
    }
  }

  get checked() {
    return this._checkbox.checked;
  }

  set checked(value) {
    return this._checkbox.checked = value;
  }
}

customElements.define("labelled-checkbox", LabelledCheckbox);
