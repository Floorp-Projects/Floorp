/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Creates form autofill preferences group.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

const PREF_AUTOFILL_ENABLED = "browser.formautofill.enabled";

function FormAutofillPreferences() {
  this.bundle = Services.strings.createBundle(
                  "chrome://formautofill/locale/formautofill.properties");
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
   * Check if the current page is Preferences/Privacy.
   *
   * @returns {boolean}
   */
  get isPrivacyPane() {
    return this.refs.document.location.href == "about:preferences#privacy";
  },

  /**
   * Create the Form Autofill preference group.
   *
   * @param   {XULDocument} document
   * @returns {XULElement}
   */
  init(document) {
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    let formAutofillGroup = document.createElementNS(XUL_NS, "groupbox");
    let caption = document.createElementNS(XUL_NS, "caption");
    let captionLabel = document.createElementNS(XUL_NS, "label");
    let hbox = document.createElementNS(XUL_NS, "hbox");
    let enabledCheckbox = document.createElementNS(XUL_NS, "checkbox");
    let spacer = document.createElementNS(XUL_NS, "spacer");
    let savedProfilesBtn = document.createElementNS(XUL_NS, "button");

    this.refs = {
      document,
      formAutofillGroup,
      enabledCheckbox,
      savedProfilesBtn,
    };

    formAutofillGroup.id = "formAutofillGroup";
    formAutofillGroup.hidden = !this.isPrivacyPane;
    // Use .setAttribute because HTMLElement.dataset is not available on XUL elements
    formAutofillGroup.setAttribute("data-category", "panePrivacy");

    captionLabel.textContent = this.bundle.GetStringFromName("preferenceGroupTitle");
    savedProfilesBtn.setAttribute("label", this.bundle.GetStringFromName("savedProfiles"));
    enabledCheckbox.setAttribute("label", this.bundle.GetStringFromName("enableProfileAutofill"));

    // Manually set the checked state
    if (this.isAutofillEnabled) {
      enabledCheckbox.setAttribute("checked", true);
    }

    spacer.flex = 1;

    formAutofillGroup.appendChild(caption);
    caption.appendChild(captionLabel);
    formAutofillGroup.appendChild(hbox);
    hbox.appendChild(enabledCheckbox);
    hbox.appendChild(spacer);
    hbox.appendChild(savedProfilesBtn);

    this.attachEventListeners();

    return formAutofillGroup;
  },

  /**
   * Remove event listeners and the preference group.
   */
  uninit() {
    this.detachEventListeners();
    this.refs.formAutofillGroup.remove();
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

        if (target == this.refs.enabledCheckbox) {
          // Set preference directly instead of relying on <Preference>
          Services.prefs.setBoolPref(PREF_AUTOFILL_ENABLED, target.checked);
        } else if (target == this.refs.savedProfilesBtn) {
          // TODO: Open Saved Profiles dialog
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

this.EXPORTED_SYMBOLS = ["FormAutofillPreferences"];
