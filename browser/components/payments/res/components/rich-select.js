/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";
import RichOption from "./rich-option.js";

/**
 * <rich-select>
 *  <rich-option></rich-option>
 * </rich-select>
 *
 * Note: The only supported way to change the selected option is via the
 *       `value` setter.
 */
export default class RichSelect extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "disabled",
      "hidden",
    ];
  }

  constructor() {
    super();
    this.popupBox = document.createElement("select");
    this.popupBox.addEventListener("change", this);
  }

  connectedCallback() {
    this.tabIndex = 0;
    this.appendChild(this.popupBox);
    this.render();
  }

  get selectedOption() {
    return this.getOptionByValue(this.value);
  }

  get value() {
    return this.popupBox.value;
  }

  set value(guid) {
    this.popupBox.value = guid;
    this.render();
  }

  getOptionByValue(value) {
    return this.popupBox.querySelector(`:scope > [value="${CSS.escape(value)}"]`);
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        // Since the render function depends on the popupBox's value, we need to
        // re-render if the value changes.
        this.render();
        break;
      }
    }
  }

  render() {
    let selectedRichOption = this.querySelector(":scope > .rich-select-selected-option");
    if (selectedRichOption) {
      selectedRichOption.remove();
    }

    if (this.value) {
      let optionType = this.getAttribute("option-type");
      if (selectedRichOption.localName != optionType) {
        selectedRichOption = document.createElement(optionType);
      }

      let option = this.getOptionByValue(this.value);
      let attributeNames = selectedRichOption.constructor.recordAttributes;
      for (let attributeName of attributeNames) {
        let attributeValue = option.getAttribute(attributeName);
        if (attributeValue) {
          selectedRichOption.setAttribute(attributeName, attributeValue);
        } else {
          selectedRichOption.removeAttribute(attributeName);
        }
      }
    } else {
      selectedRichOption = new RichOption();
      selectedRichOption.textContent = "(None selected)";
    }
    selectedRichOption.classList.add("rich-select-selected-option");
    selectedRichOption = this.appendChild(selectedRichOption);
  }
}

customElements.define("rich-select", RichSelect);
