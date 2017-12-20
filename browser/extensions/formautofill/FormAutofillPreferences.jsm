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

const {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
  MANAGE_ADDRESSES_KEYWORDS,
  EDIT_ADDRESS_KEYWORDS,
  MANAGE_CREDITCARDS_KEYWORDS,
  EDIT_CREDITCARD_KEYWORDS,
} = FormAutofillUtils;
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
    let learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "autofill-card-address";
    let formAutofillGroup = document.createElementNS(XUL_NS, "vbox");
    let addressAutofill = document.createElementNS(XUL_NS, "hbox");
    let addressAutofillCheckboxGroup = document.createElementNS(XUL_NS, "description");
    let addressAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
    let addressAutofillLearnMore = document.createElementNS(XUL_NS, "label");
    let savedAddressesBtn = document.createElementNS(XUL_NS, "button");
    // Wrappers are used to properly compute the search tooltip positions
    let savedAddressesBtnWrapper = document.createElementNS(XUL_NS, "hbox");
    let savedCreditCardsBtnWrapper = document.createElementNS(XUL_NS, "hbox");

    savedAddressesBtn.className = "accessory-button";
    addressAutofillCheckbox.className = "tail-with-learn-more";
    addressAutofillLearnMore.className = "learnMore text-link";

    formAutofillGroup.id = "formAutofillGroup";
    addressAutofill.id = "addressAutofill";
    addressAutofillLearnMore.id = "addressAutofillLearnMore";

    addressAutofill.setAttribute("data-subcategory", "address-autofill");
    addressAutofillLearnMore.setAttribute("value", this.bundle.GetStringFromName("learnMoreLabel"));
    addressAutofillCheckbox.setAttribute("label", this.bundle.GetStringFromName("autofillAddressesCheckbox"));
    savedAddressesBtn.setAttribute("label", this.bundle.GetStringFromName("savedAddressesBtnLabel"));
    // Align the start to keep the savedAddressesBtn as original size
    // when addressAutofillCheckboxGroup's height is changed by a longer l10n string
    savedAddressesBtnWrapper.setAttribute("align", "start");

    addressAutofillLearnMore.setAttribute("href", learnMoreURL);

    // Add preferences search support
    savedAddressesBtn.setAttribute("searchkeywords", MANAGE_ADDRESSES_KEYWORDS.concat(EDIT_ADDRESS_KEYWORDS)
                                                       .map(key => this.bundle.GetStringFromName(key)).join("\n"));

    // Manually set the checked state
    if (FormAutofillUtils.isAutofillAddressesEnabled) {
      addressAutofillCheckbox.setAttribute("checked", true);
    }

    addressAutofillCheckboxGroup.flex = 1;

    formAutofillGroup.appendChild(addressAutofill);
    addressAutofill.appendChild(addressAutofillCheckboxGroup);
    addressAutofillCheckboxGroup.appendChild(addressAutofillCheckbox);
    addressAutofillCheckboxGroup.appendChild(addressAutofillLearnMore);
    addressAutofill.appendChild(savedAddressesBtnWrapper);
    savedAddressesBtnWrapper.appendChild(savedAddressesBtn);

    this.refs = {
      formAutofillGroup,
      addressAutofillCheckbox,
      savedAddressesBtn,
    };

    if (FormAutofillUtils.isAutofillCreditCardsAvailable) {
      let creditCardAutofill = document.createElementNS(XUL_NS, "hbox");
      let creditCardAutofillCheckboxGroup = document.createElementNS(XUL_NS, "description");
      let creditCardAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
      let creditCardAutofillLearnMore = document.createElementNS(XUL_NS, "label");
      let savedCreditCardsBtn = document.createElementNS(XUL_NS, "button");
      savedCreditCardsBtn.className = "accessory-button";
      creditCardAutofillCheckbox.className = "tail-with-learn-more";
      creditCardAutofillLearnMore.className = "learnMore text-link";

      creditCardAutofill.id = "creditCardAutofill";
      creditCardAutofillLearnMore.id = "creditCardAutofillLearnMore";

      creditCardAutofill.setAttribute("data-subcategory", "credit-card-autofill");
      creditCardAutofillLearnMore.setAttribute("value", this.bundle.GetStringFromName("learnMoreLabel"));
      creditCardAutofillCheckbox.setAttribute("label", this.bundle.GetStringFromName("autofillCreditCardsCheckbox"));
      savedCreditCardsBtn.setAttribute("label", this.bundle.GetStringFromName("savedCreditCardsBtnLabel"));
      // Align the start to keep the savedCreditCardsBtn as original size
      // when creditCardAutofillCheckboxGroup's height is changed by a longer l10n string
      savedCreditCardsBtnWrapper.setAttribute("align", "start");

      creditCardAutofillLearnMore.setAttribute("href", learnMoreURL);

      // Add preferences search support
      savedCreditCardsBtn.setAttribute("searchkeywords", MANAGE_CREDITCARDS_KEYWORDS.concat(EDIT_CREDITCARD_KEYWORDS)
                                                           .map(key => this.bundle.GetStringFromName(key)).join("\n"));

      // Manually set the checked state
      if (FormAutofillUtils.isAutofillCreditCardsEnabled) {
        creditCardAutofillCheckbox.setAttribute("checked", true);
      }

      creditCardAutofillCheckboxGroup.flex = 1;

      formAutofillGroup.appendChild(creditCardAutofill);
      creditCardAutofill.appendChild(creditCardAutofillCheckboxGroup);
      creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillCheckbox);
      creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillLearnMore);
      creditCardAutofill.appendChild(savedCreditCardsBtnWrapper);
      savedCreditCardsBtnWrapper.appendChild(savedCreditCardsBtn);

      this.refs.creditCardAutofillCheckbox = creditCardAutofillCheckbox;
      this.refs.savedCreditCardsBtn = savedCreditCardsBtn;
    }
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
