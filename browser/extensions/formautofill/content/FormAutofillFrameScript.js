/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill frame script.
 */

"use strict";

/* global content */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillContent.jsm");

/**
 * Handles content's interactions for the frame.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillFrameScript = {
  init() {
    addEventListener("DOMContentLoaded", this);
    addMessageListener("FormAutofill:PreviewProfile", this);
    addMessageListener("FormAutoComplete:PopupClosed", this);
  },

  handleEvent(evt) {
    if (!evt.isTrusted) {
      return;
    }

    if (!Services.prefs.getBoolPref("browser.formautofill.enabled")) {
      return;
    }

    switch (evt.type) {
      case "DOMContentLoaded": {
        let doc = evt.target;
        if (!(doc instanceof Ci.nsIDOMHTMLDocument)) {
          return;
        }
        FormAutofillContent.identifyAutofillFields(doc);
        break;
      }
    }
  },

  receiveMessage(message) {
    if (!Services.prefs.getBoolPref("browser.formautofill.enabled")) {
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
