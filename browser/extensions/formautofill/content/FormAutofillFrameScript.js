/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill frame script.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Handles content's interactions for the frame.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillFrameScript = {
  init() {
    addEventListener("DOMContentLoaded", this);
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
        this.FormAutofillContent._identifyAutofillFields(doc);
        break;
      }
    }
  },
};

XPCOMUtils.defineLazyModuleGetter(FormAutofillFrameScript, "FormAutofillContent",
                                  "resource://formautofill/FormAutofillContent.jsm");

FormAutofillFrameScript.init();
