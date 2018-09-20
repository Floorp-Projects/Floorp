/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";
import RichOption from "./rich-option.js";
/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * Up to two-line address display. After bug 1475684 this will also be used for
 * the single-line <option> substitute too.
 *
 * <rich-select>
 *  <address-option guid="98hgvnbmytfc"
 *                  address-level1="MI"
 *                  address-level2="Some City"
 *                  email="foo@example.com"
 *                  country="USA"
 *                  name="Jared Wein"
 *                  postal-code="90210"
 *                  street-address="1234 Anywhere St"
 *                  tel="+1 650 555-5555"></address-option>
 * </rich-select>
 *
 * Attribute names follow FormAutofillStorage.jsm.
 */

export default class AddressOption extends ObservedPropertiesMixin(RichOption) {
  static get recordAttributes() {
    return [
      "address-level1",
      "address-level2",
      "country",
      "email",
      "guid",
      "name",
      "organization",
      "postal-code",
      "street-address",
      "tel",
    ];
  }

  static get observedAttributes() {
    return RichOption.observedAttributes.concat(AddressOption.recordAttributes,
                                                "address-fields",
                                                "break-after-nth-field",
                                                "data-field-separator");
  }

  constructor() {
    super();

    this._line1 = document.createElement("div");
    this._line1.classList.add("line");
    this._line2 = document.createElement("div");
    this._line2.classList.add("line");

    for (let name of AddressOption.recordAttributes) {
      this[`_${name}`] = document.createElement("span");
      this[`_${name}`].classList.add(name);
      // XXX Bug 1490816: Use appropriate strings
      let missingValueString = name.replace(/(-|^)([a-z])/g, ($0, $1, $2) => {
        return $1.replace("-", " ") + $2.toUpperCase();
      }) + " Missing";
      this[`_${name}`].dataset.missingString = missingValueString;
    }
  }

  connectedCallback() {
    this.appendChild(this._line1);
    this.appendChild(this._line2);
    super.connectedCallback();
  }

  static formatSingleLineLabel(address, addressFields) {
    return PaymentDialogUtils.getAddressLabel(address, addressFields);
  }

  get requiredFields() {
    if (this.hasAttribute("address-fields")) {
      let names = this.getAttribute("address-fields").trim().split(/\s+/);
      if (names.length) {
        return names;
      }
    }

    return [
      // "address-level1", // TODO: bug 1481481 - not required for some countries e.g. DE
      "address-level2",
      "country",
      "name",
      "postal-code",
      "street-address",
    ];
  }

  render() {
    // Clear the lines of the fields so we can append only the ones still
    // visible in the correct order below.
    this._line1.textContent = "";
    this._line2.textContent = "";

    // Fill the fields with their text/strings.
    // Fall back to empty strings to prevent 'null' from appearing.
    for (let name of AddressOption.recordAttributes) {
      let camelCaseName = super.constructor.kebabToCamelCase(name);
      let fieldEl = this[`_${name}`];
      fieldEl.textContent = this[camelCaseName] || "";
    }

    let {fieldsOrder} = PaymentDialogUtils.getFormFormat(this.country);
    // A subset of the requested fields may be returned if the fields don't apply to the country.
    let requestedVisibleFields = this.addressFields || "mailing-address";
    let visibleFields = EditAddress.computeVisibleFields(fieldsOrder, requestedVisibleFields);
    let visibleFieldCount = 0;
    let requiredFields = this.requiredFields;
    // Start by populating line 1
    let lineEl = this._line1;
    // Which field number to start line 2 after.
    let breakAfterNthField = this.breakAfterNthField || 2;

    // Now actually place the fields in the proper place on the lines.
    for (let field of visibleFields) {
      let fieldEl = this[`_${field.fieldId}`];
      if (!fieldEl) {
        log.warn(`address-option render: '${field.fieldId}' doesn't exist`);
        continue;
      }

      if (!fieldEl.textContent && !requiredFields.includes(field.fieldId)) {
        // The field is empty and we don't need to show "Missing …" so don't append.
        continue;
      }

      if (lineEl.children.length > 0) {
        lineEl.append(this.dataset.fieldSeparator);
      }
      lineEl.appendChild(fieldEl);

      // Add a break after this field, if requested.
      if (++visibleFieldCount == breakAfterNthField) {
        lineEl = this._line2;
      }
    }
  }
}

customElements.define("address-option", AddressOption);
