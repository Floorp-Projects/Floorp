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
const PREF_AUTOFILL_ENABLED = "extensions.formautofill.addresses.enabled";
const BUNDLE_URI = "chrome://formautofill/locale/formautofill.properties";
const MANAGE_ADDRESSES_URL = "chrome://formautofill/content/manageAddresses.xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

function FormAutofillPreferences({useOldOrganization}) {
  this.useOldOrganization = useOldOrganization;
  this.bundle = Services.strings.createBundle(BUNDLE_URI);
}

FormAutofillPreferences.prototype = {
  /**
   * Check if Form Autofill feature is enabled.
   *
   * @returns {boolean}
   */
  get isAutofillEnabled() {
    return Services.prefs.getBoolPref(PREF_AUTOFILL_ENABLED);
  },

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
    let formAutofillGroup;
    let addressAutofill = document.createElementNS(XUL_NS, "hbox");
    let addressAutofillCheckbox = document.createElementNS(XUL_NS, "checkbox");
    let savedAddressesBtn = document.createElementNS(XUL_NS, "button");

    if (this.useOldOrganization) {
      let caption = document.createElementNS(XUL_NS, "caption");
      let captionLabel = document.createElementNS(XUL_NS, "label");

      formAutofillGroup = document.createElementNS(XUL_NS, "groupbox");
      formAutofillGroup.hidden = document.location.href != "about:preferences#privacy";
      // Use .setAttribute because HTMLElement.dataset is not available on XUL elements
      formAutofillGroup.setAttribute("data-category", "panePrivacy");
      formAutofillGroup.appendChild(caption);
      caption.appendChild(captionLabel);
      captionLabel.textContent = this.bundle.GetStringFromName("preferenceGroupTitle");
    } else {
      formAutofillGroup = document.createElementNS(XUL_NS, "vbox");
      savedAddressesBtn.className = "accessory-button";
    }

    this.refs = {
      formAutofillGroup,
      addressAutofillCheckbox,
      savedAddressesBtn,
    };

    formAutofillGroup.id = "formAutofillGroup";
    addressAutofill.id = "addressAutofill";
    savedAddressesBtn.setAttribute("label", this.bundle.GetStringFromName("savedAddresses"));
    addressAutofillCheckbox.setAttribute("label", this.bundle.GetStringFromName("enableAddressAutofill"));

    // Manually set the checked state
    if (this.isAutofillEnabled) {
      addressAutofillCheckbox.setAttribute("checked", true);
    }

    addressAutofillCheckbox.flex = 1;

    formAutofillGroup.appendChild(addressAutofill);
    addressAutofill.appendChild(addressAutofillCheckbox);
    addressAutofill.appendChild(savedAddressesBtn);
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
          Services.prefs.setBoolPref(PREF_AUTOFILL_ENABLED, target.checked);
        } else if (target == this.refs.savedAddressesBtn) {
          target.ownerGlobal.gSubDialog.open(MANAGE_ADDRESSES_URL);
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
