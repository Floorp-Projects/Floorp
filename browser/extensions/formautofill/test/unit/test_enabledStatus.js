/*
 * Test for status handling in Form Autofill Parent.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillParent.jsm");

add_task(function* test_enabledStatus_init() {
  let formAutofillParent = new FormAutofillParent();
  sinon.spy(formAutofillParent, "_onStatusChanged");

  // Default status is false before initialization
  do_check_eq(formAutofillParent._enabled, false);

  formAutofillParent.init();
  do_check_eq(formAutofillParent._onStatusChanged.called, true);

  formAutofillParent._uninit();
});

add_task(function* test_enabledStatus_observe() {
  let formAutofillParent = new FormAutofillParent();
  sinon.stub(formAutofillParent, "_getStatus");
  sinon.spy(formAutofillParent, "_onStatusChanged");

  // _enabled = _getStatus() => No need to trigger onStatusChanged
  formAutofillParent._enabled = true;
  formAutofillParent._getStatus.returns(true);
  formAutofillParent.observe(null, "nsPref:changed", "browser.formautofill.enabled");
  do_check_eq(formAutofillParent._onStatusChanged.called, false);

  // _enabled != _getStatus() => Need to trigger onStatusChanged
  formAutofillParent._getStatus.returns(false);
  formAutofillParent.observe(null, "nsPref:changed", "browser.formautofill.enabled");
  do_check_eq(formAutofillParent._onStatusChanged.called, true);
});

add_task(function* test_enabledStatus_getStatus() {
  let formAutofillParent = new FormAutofillParent();
  do_register_cleanup(function cleanup() {
    Services.prefs.clearUserPref("browser.formautofill.enabled");
  });

  Services.prefs.setBoolPref("browser.formautofill.enabled", true);
  do_check_eq(formAutofillParent._getStatus(), true);

  Services.prefs.setBoolPref("browser.formautofill.enabled", false);
  do_check_eq(formAutofillParent._getStatus(), false);
});
