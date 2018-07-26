/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Injects the form autofill section into about:preferences.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillPreferences"];

// Add addresses enabled flag in telemetry environment for recording the number of
// users who disable/enable the address autofill feature.
const BUNDLE_URI = "chrome://formautofill/locale/formautofill.properties";
const MANAGE_ADDRESSES_URL = "chrome://formautofill/content/manageAddresses.xhtml";
const MANAGE_CREDITCARDS_URL = "chrome://formautofill/content/manageCreditCards.xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://formautofill/FormAutofill.jsm");
ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

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
FormAutofill.defineLazyLogGetter(this, EXPORTED_SYMBOLS[0]);

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
    let addressAutofillCheckboxGroup = document.createElementNS(XUL_NS, "hbox");
    let addressAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
    let addressAutofillCheckboxLabel = document.createElementNS(XUL_NS, "label");
    let addressAutofillCheckboxLabelSpacer = document.createElementNS(XUL_NS, "spacer");
    let addressAutofillLearnMore = document.createElementNS(XUL_NS, "label");
    let savedAddressesBtn = document.createElementNS(XUL_NS, "button");
    // Wrappers are used to properly compute the search tooltip positions
    let savedAddressesBtnWrapper = document.createElementNS(XUL_NS, "hbox");
    let savedCreditCardsBtnWrapper = document.createElementNS(XUL_NS, "hbox");

    savedAddressesBtn.className = "accessory-button";
    addressAutofillCheckboxLabelSpacer.className = "tail-with-learn-more";
    addressAutofillLearnMore.className = "learnMore text-link";

    formAutofillGroup.id = "formAutofillGroup";
    addressAutofill.id = "addressAutofill";
    addressAutofillLearnMore.id = "addressAutofillLearnMore";

    addressAutofill.setAttribute("data-subcategory", "address-autofill");
    addressAutofillCheckboxLabel.textContent = this.bundle.GetStringFromName("autofillAddressesCheckbox");
    addressAutofillCheckbox.setAttribute("aria-label", addressAutofillCheckboxLabel.textContent);
    addressAutofillLearnMore.textContent = this.bundle.GetStringFromName("learnMoreLabel");
    savedAddressesBtn.setAttribute("label", this.bundle.GetStringFromName("savedAddressesBtnLabel"));
    // Align the start to keep the savedAddressesBtn as original size
    // when addressAutofillCheckboxGroup's height is changed by a longer l10n string
    savedAddressesBtnWrapper.setAttribute("align", "start");

    addressAutofillLearnMore.setAttribute("href", learnMoreURL);

    // Add preferences search support
    savedAddressesBtn.setAttribute("searchkeywords", MANAGE_ADDRESSES_KEYWORDS.concat(EDIT_ADDRESS_KEYWORDS)
                                                       .map(key => this.bundle.GetStringFromName(key)).join("\n"));

    // Manually set the checked state
    if (FormAutofill.isAutofillAddressesEnabled) {
      addressAutofillCheckbox.setAttribute("checked", true);
    }

    addressAutofillCheckboxGroup.align = "center";
    addressAutofillCheckboxGroup.flex = 1;
    addressAutofillCheckboxLabel.flex = 1;

    formAutofillGroup.appendChild(addressAutofill);
    addressAutofill.appendChild(addressAutofillCheckboxGroup);
    addressAutofillCheckboxGroup.appendChild(addressAutofillCheckbox);
    addressAutofillCheckboxGroup.appendChild(addressAutofillCheckboxLabel);
    addressAutofillCheckboxLabel.appendChild(addressAutofillCheckboxLabelSpacer);
    addressAutofillCheckboxLabel.appendChild(addressAutofillLearnMore);
    addressAutofill.appendChild(savedAddressesBtnWrapper);
    savedAddressesBtnWrapper.appendChild(savedAddressesBtn);

    this.refs = {
      formAutofillGroup,
      addressAutofillCheckbox,
      addressAutofillCheckboxLabel,
      savedAddressesBtn,
    };

    if (FormAutofill.isAutofillCreditCardsAvailable) {
      let creditCardAutofill = document.createElementNS(XUL_NS, "hbox");
      let creditCardAutofillCheckboxGroup = document.createElementNS(XUL_NS, "hbox");
      let creditCardAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
      let creditCardAutofillCheckboxLabel = document.createElementNS(XUL_NS, "label");
      let creditCardAutofillCheckboxLabelSpacer = document.createElementNS(XUL_NS, "spacer");
      let creditCardAutofillLearnMore = document.createElementNS(XUL_NS, "label");
      let savedCreditCardsBtn = document.createElementNS(XUL_NS, "button");
      savedCreditCardsBtn.className = "accessory-button";
      creditCardAutofillCheckboxLabelSpacer.className = "tail-with-learn-more";
      creditCardAutofillLearnMore.className = "learnMore text-link";

      creditCardAutofill.id = "creditCardAutofill";
      creditCardAutofillLearnMore.id = "creditCardAutofillLearnMore";

      creditCardAutofill.setAttribute("data-subcategory", "credit-card-autofill");
      creditCardAutofillCheckboxLabel.textContent = this.bundle.GetStringFromName("autofillCreditCardsCheckbox");
      creditCardAutofillCheckbox.setAttribute("aria-label", creditCardAutofillCheckboxLabel.textContent);
      creditCardAutofillLearnMore.textContent = this.bundle.GetStringFromName("learnMoreLabel");
      savedCreditCardsBtn.setAttribute("label", this.bundle.GetStringFromName("savedCreditCardsBtnLabel"));
      // Align the start to keep the savedCreditCardsBtn as original size
      // when creditCardAutofillCheckboxGroup's height is changed by a longer l10n string
      savedCreditCardsBtnWrapper.setAttribute("align", "start");

      creditCardAutofillLearnMore.setAttribute("href", learnMoreURL);

      // Add preferences search support
      savedCreditCardsBtn.setAttribute("searchkeywords", MANAGE_CREDITCARDS_KEYWORDS.concat(EDIT_CREDITCARD_KEYWORDS)
                                                           .map(key => this.bundle.GetStringFromName(key)).join("\n"));

      // Manually set the checked state
      if (FormAutofill.isAutofillCreditCardsEnabled) {
        creditCardAutofillCheckbox.setAttribute("checked", true);
      }

      creditCardAutofillCheckboxGroup.align = "center";
      creditCardAutofillCheckboxGroup.flex = 1;
      creditCardAutofillCheckboxLabel.flex = 1;

      formAutofillGroup.appendChild(creditCardAutofill);
      creditCardAutofill.appendChild(creditCardAutofillCheckboxGroup);
      creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillCheckbox);
      creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillCheckboxLabel);
      creditCardAutofillCheckboxLabel.appendChild(creditCardAutofillCheckboxLabelSpacer);
      creditCardAutofillCheckboxLabel.appendChild(creditCardAutofillLearnMore);
      creditCardAutofill.appendChild(savedCreditCardsBtnWrapper);
      savedCreditCardsBtnWrapper.appendChild(savedCreditCardsBtn);

      this.refs.creditCardAutofillCheckbox = creditCardAutofillCheckbox;
      this.refs.creditCardAutofillCheckboxLabel = creditCardAutofillCheckboxLabel;
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
      case "click": {
        let target = event.target;

        if (target == this.refs.addressAutofillCheckboxLabel) {
          let pref = FormAutofill.isAutofillAddressesEnabled;
          Services.prefs.setBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF, !pref);
          this.refs.addressAutofillCheckbox.checked = !pref;
        } else if (target == this.refs.creditCardAutofillCheckboxLabel) {
          let pref = FormAutofill.isAutofillCreditCardsEnabled;
          Services.prefs.setBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF, !pref);
          this.refs.creditCardAutofillCheckbox.checked = !pref;
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
    this.refs.formAutofillGroup.addEventListener("click", this);
  },

  /**
   * Remove event listener
   */
  detachEventListeners() {
    this.refs.formAutofillGroup.removeEventListener("command", this);
    this.refs.formAutofillGroup.removeEventListener("click", this);
  },
};
