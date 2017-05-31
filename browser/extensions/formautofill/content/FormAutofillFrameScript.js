/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill frame script.
 */

"use strict";

/* eslint-env mozilla/frame-script */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillContent.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

/**
 * Handles content's interactions for the frame.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillFrameScript = {
  init() {
    addEventListener("focusin", this);
    addMessageListener("FormAutofill:PreviewProfile", this);
    addMessageListener("FormAutoComplete:PopupClosed", this);
  },

  handleEvent(evt) {
    if (!evt.isTrusted) {
      return;
    }

    if (!Services.prefs.getBoolPref("extensions.formautofill.addresses.enabled")) {
      return;
    }

    switch (evt.type) {
      case "focusin": {
        let element = evt.target;
        if (!FormAutofillUtils.isFieldEligibleForAutofill(element)) {
          return;
        }
        FormAutofillContent.identifyAutofillFields(element.ownerDocument);
        break;
      }
    }
  },

  receiveMessage(message) {
    if (!Services.prefs.getBoolPref("extensions.formautofill.addresses.enabled")) {
      return;
    }

    switch (message.name) {
      case "FormAutofill:PreviewProfile":
      case "FormAutoComplete:PopupClosed":
        FormAutofillContent._previewProfile(content.document);
        break;
    }
  },
};

FormAutofillFrameScript.init();
