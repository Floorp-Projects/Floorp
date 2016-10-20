/**
 * Provides infrastructure for automated login components tests.
 */

 /* exported importAutofillModule */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/MockDocument.jsm");

// Load the module by Service newFileURI API for running extension's XPCShell test
function importAutofillModule(module) {
  return Cu.import(Services.io.newFileURI(do_get_file(module)).spec);
}

add_task(function* test_common_initialize() {
  Services.prefs.setBoolPref("dom.forms.autocomplete.experimental", true);

  // Clean up after every test.
  do_register_cleanup(() => {
    Services.prefs.setBoolPref("dom.forms.autocomplete.experimental", false);
  });
});
