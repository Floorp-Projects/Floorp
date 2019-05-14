/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class ReflectedFluentElement extends HTMLElement {
  _isReflectedAttributePresent(attr) {
    return this.constructor.reflectedFluentIDs.includes(attr.name);
  }

  /* This function should be called to apply any localized strings that Fluent
     may have applied to the element before the custom element was defined. */
  reflectFluentStrings() {
    for (let reflectedFluentID of this.constructor.reflectedFluentIDs) {
      if (this.hasAttribute(reflectedFluentID)) {
        if (this.handleSpecialCaseFluentString &&
            this.handleSpecialCaseFluentString(reflectedFluentID)) {
          continue;
        }

        let attrValue = this.getAttribute(reflectedFluentID);
        // Strings that are reflected to their shadowed element are assigned
        // to an attribute name that matches a className on the element.
        let shadowedElement = this.shadowRoot.querySelector("." + reflectedFluentID);
        shadowedElement.textContent = attrValue;
      }
    }
  }

  /* Fluent doesn't handle localizing into Shadow DOM yet so strings
     need to get reflected in to their targeted element. */
  attributeChangedCallback(attr, oldValue, newValue) {
    if (!this.shadowRoot) {
      return;
    }

    // Don't respond to attribute changes that aren't related to locale text.
    if (!this.constructor.reflectedFluentIDs.includes(attr)) {
      return;
    }

    if (this.handleSpecialCaseFluentString &&
        this.handleSpecialCaseFluentString(attr)) {
      return;
    }

    // Strings that are reflected to their shadowed element are assigned
    // to an attribute name that matches a className on the element.
    let shadowedElement = this.shadowRoot.querySelector("." + attr);
    shadowedElement.textContent = newValue;
  }
}
customElements.define("reflected-fluent-element", ReflectedFluentElement);
