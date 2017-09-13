/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Injects the form autofill section into about:preferences.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillPreferences"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
// Add addresses enabled flag in telemetry environment for recording the number of
// users who disable/enable the address autofill feature.
const BUNDLE_URI = "chrome://formautofill/locale/formautofill.properties";
const MANAGE_ADDRESSES_URL = "chrome://formautofill/content/manageAddresses.xhtml";
const MANAGE_CREDITCARDS_URL = "chrome://formautofill/content/manageCreditCards.xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

const {ENABLED_AUTOFILL_ADDRESSES_PREF, ENABLED_AUTOFILL_CREDITCARDS_PREF} = FormAutofillUtils;
// Add credit card enabled flag in telemetry environment for recording the number of
// users who disable/enable the credit card autofill feature.

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

function FormAutofillPreferences() {
  this.bundle = Services.strings.createBundle(BUNDLE_URI);
}

FormAutofillPreferences.prototype = {
  /**
   * Create the Form Autofill preference group.
   *
   * @param   {XULDocument} document
   * @returns {XULElement}
   */
  init(document) {
    this.createPreferenceGroup(document);
    this.attachEventListeners();

    return this.refs.formAutofillGroup;
  },

  /**
   * Remove event listeners and the preference group.
   */
  uninit() {
    this.detachEventListeners();
    this.refs.formAutofillGroup.remove();
  },

  /**
   * Create Form Autofill preference group
   *
   * @param  {XULDocument} document
   */
  createPreferenceGroup(document) {
    let formAutofillGroup = document.createElementNS(XUL_NS, "vbox");
    let addressAutofill = document.createElementNS(XUL_NS, "hbox");
    let addressAutofillCheckboxGroup = document.createElementNS(XUL_NS, "description");
    let addressAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
    let addressAutofillLearnMore = document.createElementNS(XUL_NS, "label");
    let savedAddressesBtn = document.createElementNS(XUL_NS, "button");
    let creditCardAutofill = document.createElementNS(XUL_NS, "hbox");
    let creditCardAutofillCheckboxGroup = document.createElementNS(XUL_NS, "description");
    let creditCardAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
    let creditCardAutofillLearnMore = document.createElementNS(XUL_NS, "label");
    let savedCreditCardsBtn = document.createElementNS(XUL_NS, "button");

    savedAddressesBtn.className = "accessory-button";
    savedCreditCardsBtn.className = "accessory-button";
    addressAutofillLearnMore.className = "learnMore text-link";
    creditCardAutofillLearnMore.className = "learnMore text-link";

    this.refs = {
      formAutofillGroup,
      addressAutofillCheckbox,
      savedAddressesBtn,
      creditCardAutofillCheckbox,
      savedCreditCardsBtn,
    };

    formAutofillGroup.id = "formAutofillGroup";
    addressAutofill.id = "addressAutofill";
    addressAutofillLearnMore.id = "addressAutofillLearnMore";
    creditCardAutofill.id = "creditCardAutofill";
    creditCardAutofillLearnMore.id = "creditCardAutofillLearnMore";

    addressAutofillLearnMore.setAttribute("value", this.bundle.GetStringFromName("learnMore"));
    addressAutofillCheckbox.setAttribute("label", this.bundle.GetStringFromName("enableAddressAutofill"));
    savedAddressesBtn.setAttribute("label", this.bundle.GetStringFromName("savedAddresses"));
    creditCardAutofillLearnMore.setAttribute("value", this.bundle.GetStringFromName("learnMore"));
    creditCardAutofillCheckbox.setAttribute("label", this.bundle.GetStringFromName("enableCreditCardAutofill"));
    savedCreditCardsBtn.setAttribute("label", this.bundle.GetStringFromName("savedCreditCards"));

    let learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "autofill-card-address";
    addressAutofillLearnMore.setAttribute("href", learnMoreURL);
    creditCardAutofillLearnMore.setAttribute("href", learnMoreURL);

    // Manually set the checked state
    if (FormAutofillUtils.isAutofillAddressesEnabled) {
      addressAutofillCheckbox.setAttribute("checked", true);
    }
    if (FormAutofillUtils.isAutofillCreditCardsEnabled) {
      creditCardAutofillCheckbox.setAttribute("checked", true);
    }

    addressAutofillCheckboxGroup.flex = 1;
    creditCardAutofillCheckboxGroup.flex = 1;

    formAutofillGroup.appendChild(addressAutofill);
    addressAutofill.appendChild(addressAutofillCheckboxGroup);
    addressAutofillCheckboxGroup.appendChild(addressAutofillCheckbox);
    addressAutofillCheckboxGroup.appendChild(addressAutofillLearnMore);
    addressAutofill.appendChild(savedAddressesBtn);
    formAutofillGroup.appendChild(creditCardAutofill);
    creditCardAutofill.appendChild(creditCardAutofillCheckboxGroup);
    creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillCheckbox);
    creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillLearnMore);
    creditCardAutofill.appendChild(savedCreditCardsBtn);
  },

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "command": {
        let target = event.target;

        if (target == this.refs.addressAutofillCheckbox) {
          // Set preference directly instead of relying on <Preference>
          Services.prefs.setBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF, target.checked);
        } else if (target == this.refs.creditCardAutofillCheckbox) {
          Services.prefs.setBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF, target.checked);
        } else if (target == this.refs.savedAddressesBtn) {
          target.ownerGlobal.gSubDialog.open(MANAGE_ADDRESSES_URL);
        } else if (target == this.refs.savedCreditCardsBtn) {
          target.ownerGlobal.gSubDialog.open(MANAGE_CREDITCARDS_URL);
        }
        break;
      }
    }
  },

  /**
   * Attach event listener
   */
  attachEventListeners() {
    this.refs.formAutofillGroup.addEventListener("command", this);
  },

  /**
   * Remove event listener
   */
  detachEventListeners() {
    this.refs.formAutofillGroup.removeEventListener("command", this);
  },
};
