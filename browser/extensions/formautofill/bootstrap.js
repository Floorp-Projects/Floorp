/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported startup, shutdown, install, uninstall */

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillParent",
                                  "resource://formautofill/FormAutofillParent.jsm");

function startup() {
  // Besides this pref, we'll need dom.forms.autocomplete.experimental enabled
  // as well to make sure form autocomplete works correctly.
  if (!Services.prefs.getBoolPref("browser.formautofill.experimental")) {
    return;
  }

  FormAutofillParent.init();
  Services.mm.loadFrameScript("chrome://formautofill/content/FormAutofillContent.js", true);
}

function shutdown() {}
function install() {}
function uninstall() {}
