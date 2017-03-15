/*
 * Test for status handling in Form Autofill Parent.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillParent.jsm");

add_task(function* test_enabledStatus_init() {
  let formAutofillParent = new FormAutofillParent();
  sinon.spy(formAutofillParent, "_setStatus");

  // Default status is false before initialization
  do_check_eq(formAutofillParent._enabled, false);
  do_check_eq(Services.ppmm.initialProcessData.autofillEnabled, undefined);

  formAutofillParent.init();
  do_check_eq(formAutofillParent._setStatus.called, true);
  do_check_eq(Services.ppmm.initialProcessData.autofillEnabled, false);

  formAutofillParent._uninit();
});

add_task(function* test_enabledStatus_observe() {
  let formAutofillParent = new FormAutofillParent();
  sinon.stub(formAutofillParent, "_getStatus");
  sinon.spy(formAutofillParent, "_setStatus");
  sinon.stub(formAutofillParent, "_updateSavedFieldNames");

  // _enabled = _getStatus() => No need to trigger onStatusChanged
  formAutofillParent._enabled = true;
  formAutofillParent._getStatus.returns(true);
  formAutofillParent.observe(null, "nsPref:changed", "browser.formautofill.enabled");
  do_check_eq(formAutofillParent._setStatus.called, false);

  // _enabled != _getStatus() => Need to trigger onStatusChanged
  formAutofillParent._getStatus.returns(false);
  formAutofillParent.observe(null, "nsPref:changed", "browser.formautofill.enabled");
  do_check_eq(formAutofillParent._setStatus.called, true);

  // profile added => Need to trigger onStatusChanged
  formAutofillParent._getStatus.returns(!formAutofillParent._enabled);
  formAutofillParent._setStatus.reset();
  formAutofillParent.observe(null, "formautofill-storage-changed", "add");
  do_check_eq(formAutofillParent._setStatus.called, true);

  // profile removed => Need to trigger onStatusChanged
  formAutofillParent._getStatus.returns(!formAutofillParent._enabled);
  formAutofillParent._setStatus.reset();
  formAutofillParent.observe(null, "formautofill-storage-changed", "remove");
  do_check_eq(formAutofillParent._setStatus.called, true);

  // profile updated => no need to trigger onStatusChanged
  formAutofillParent._getStatus.returns(!formAutofillParent._enabled);
  formAutofillParent._setStatus.reset();
  formAutofillParent.observe(null, "formautofill-storage-changed", "update");
  do_check_eq(formAutofillParent._setStatus.called, false);
});

add_task(function* test_enabledStatus_getStatus() {
  let formAutofillParent = new FormAutofillParent();
  do_register_cleanup(function cleanup() {
    Services.prefs.clearUserPref("browser.formautofill.enabled");
  });

  let fakeStorage = [];
  formAutofillParent._profileStore = {
    getAll: () => fakeStorage,
  };

  // pref is enabled and profile is empty.
  Services.prefs.setBoolPref("browser.formautofill.enabled", true);
  do_check_eq(formAutofillParent._getStatus(), false);

  // pref is disabled and profile is empty.
  Services.prefs.setBoolPref("browser.formautofill.enabled", false);
  do_check_eq(formAutofillParent._getStatus(), false);

  fakeStorage = ["test-profile"];
  // pref is enabled and profile is not empty.
  Services.prefs.setBoolPref("browser.formautofill.enabled", true);
  do_check_eq(formAutofillParent._getStatus(), true);

  // pref is disabled and profile is not empty.
  Services.prefs.setBoolPref("browser.formautofill.enabled", false);
  do_check_eq(formAutofillParent._getStatus(), false);
});

add_task(function* test_enabledStatus_setStatus() {
  let formAutofillParent = new FormAutofillParent();
  sinon.spy(formAutofillParent, "_onStatusChanged");

  formAutofillParent._setStatus(true);
  do_check_eq(formAutofillParent._enabled, true);
  do_check_eq(Services.ppmm.initialProcessData.autofillEnabled, true);
  do_check_eq(formAutofillParent._onStatusChanged.called, true);

  formAutofillParent._onStatusChanged.reset();
  formAutofillParent._setStatus(false);
  do_check_eq(formAutofillParent._enabled, false);
  do_check_eq(Services.ppmm.initialProcessData.autofillEnabled, false);
  do_check_eq(formAutofillParent._onStatusChanged.called, true);
});
