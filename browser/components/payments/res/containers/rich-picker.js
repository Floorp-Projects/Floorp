/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import RichSelect from "../components/rich-select.js";

export default class RichPicker extends PaymentStateSubscriberMixin(
  HTMLElement
) {
  static get observedAttributes() {
    return ["label"];
  }

  constructor() {
    super();
    this.classList.add("rich-picker");

    this.dropdown = new RichSelect();
    this.dropdown.addEventListener("change", this);

    this.labelElement = document.createElement("label");

    this.addLink = document.createElement("a");
    this.addLink.className = "add-link";
    this.addLink.href = "javascript:void(0)";
    this.addLink.addEventListener("click", this);

    this.editLink = document.createElement("a");
    this.editLink.className = "edit-link";
    this.editLink.href = "javascript:void(0)";
    this.editLink.addEventListener("click", this);

    this.invalidLabel = document.createElement("label");
    this.invalidLabel.className = "invalid-label";
  }

  connectedCallback() {
    if (!this.dropdown.popupBox.id) {
      this.dropdown.popupBox.id =
        "select-" + Math.floor(Math.random() * 1000000);
    }
    this.labelElement.setAttribute("for", this.dropdown.popupBox.id);
    this.invalidLabel.setAttribute("for", this.dropdown.popupBox.id);

    // The document order, by default, controls tab order so keep that in mind if changing this.
    this.appendChild(this.labelElement);
    this.appendChild(this.dropdown);
    this.appendChild(this.editLink);
    this.appendChild(this.addLink);
    this.appendChild(this.invalidLabel);
    super.connectedCallback();
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name == "label") {
      this.labelElement.textContent = newValue;
    }
  }

  render(state) {
    this.editLink.hidden = !this.dropdown.value;

    let errorText = this.errorForSelectedOption(state);
    this.classList.toggle("invalid-selected-option", !!errorText);
    this.invalidLabel.textContent = errorText;
    this.addLink.textContent = this.dataset.addLinkLabel;
    this.editLink.textContent = this.dataset.editLinkLabel;
  }

  get selectedOption() {
    return this.dropdown.selectedOption;
  }

  get selectedRichOption() {
    return this.dropdown.selectedRichOption;
  }

  get requiredFields() {
    return this.selectedOption ? this.selectedOption.requiredFields || [] : [];
  }

  get fieldNames() {
    return [];
  }

  /**
   * @param {object} state Application state
   * @returns {string} Containing an error message for the picker or "" for no error.
   */
  errorForSelectedOption(state) {
    if (!this.selectedOption) {
      return "";
    }
    if (!this.dataset.invalidLabel) {
      throw new Error("data-invalid-label is required");
    }
    return this.missingFieldsOfSelectedOption().length
      ? this.dataset.invalidLabel
      : "";
  }

  missingFieldsOfSelectedOption() {
    let selectedOption = this.selectedOption;
    if (!selectedOption) {
      return [];
    }

    let fieldNames = this.selectedRichOption.requiredFields || [];

    // Return all field names that are empty or missing from the option.
    return fieldNames.filter(name => !selectedOption.getAttribute(name));
  }
}
