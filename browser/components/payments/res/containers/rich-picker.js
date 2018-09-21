/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import RichSelect from "../components/rich-select.js";

export default class RichPicker extends PaymentStateSubscriberMixin(HTMLElement) {
  static get observedAttributes() {
    return ["label"];
  }

  constructor() {
    super();
    this.classList.add("rich-picker");

    this.dropdown = new RichSelect();
    this.dropdown.addEventListener("change", this);
    this.dropdown.popupBox.id = "select-" + Math.floor(Math.random() * 1000000);

    this.labelElement = document.createElement("label");
    this.labelElement.setAttribute("for", this.dropdown.popupBox.id);

    this.addLink = document.createElement("a");
    this.addLink.className = "add-link";
    this.addLink.href = "javascript:void(0)";
    this.addLink.textContent = this.dataset.addLinkLabel;
    this.addLink.addEventListener("click", this);

    this.editLink = document.createElement("a");
    this.editLink.className = "edit-link";
    this.editLink.href = "javascript:void(0)";
    this.editLink.textContent = this.dataset.editLinkLabel;
    this.editLink.addEventListener("click", this);

    this.invalidLabel = document.createElement("label");
    this.invalidLabel.className = "invalid-label";
    this.invalidLabel.setAttribute("for", this.dropdown.popupBox.id);
    this.invalidLabel.textContent = this.dataset.invalidLabel;
  }

  connectedCallback() {
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

    this.classList.toggle("invalid-selected-option",
                          !this.isSelectedOptionValid(state));
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

  isSelectedOptionValid() {
    return !this.missingFieldsOfSelectedOption().length;
  }

  missingFieldsOfSelectedOption() {
    let selectedOption = this.selectedOption;
    if (!selectedOption) {
      return [];
    }

    let fieldNames = this.selectedRichOption.requiredFields;
    // Return all field names that are empty or missing from the option.
    return fieldNames.filter(name => !selectedOption.getAttribute(name));
  }
}
