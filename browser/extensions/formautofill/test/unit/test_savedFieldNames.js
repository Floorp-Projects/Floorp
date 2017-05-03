/*
 * Test for keeping the valid fields information in initialProcessData.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillParent.jsm");
Cu.import("resource://formautofill/ProfileStorage.jsm");

add_task(function* test_profileSavedFieldNames_init() {
  let formAutofillParent = new FormAutofillParent();
  sinon.stub(formAutofillParent, "_updateSavedFieldNames");

  formAutofillParent.init();
  do_check_eq(formAutofillParent._updateSavedFieldNames.called, true);

  formAutofillParent._uninit();
});

add_task(function* test_profileSavedFieldNames_observe() {
  let formAutofillParent = new FormAutofillParent();
  sinon.stub(formAutofillParent, "_updateSavedFieldNames");

  formAutofillParent.init();

  // profile added => Need to trigger updateValidFields
  formAutofillParent.observe(null, "formautofill-storage-changed", "add");
  do_check_eq(formAutofillParent._updateSavedFieldNames.called, true);

  // profile removed => Need to trigger updateValidFields
  formAutofillParent._updateSavedFieldNames.reset();
  formAutofillParent.observe(null, "formautofill-storage-changed", "remove");
  do_check_eq(formAutofillParent._updateSavedFieldNames.called, true);

  // profile updated => no need to trigger updateValidFields
  formAutofillParent._updateSavedFieldNames.reset();
  formAutofillParent.observe(null, "formautofill-storage-changed", "update");
  do_check_eq(formAutofillParent._updateSavedFieldNames.called, false);
});

add_task(function* test_profileSavedFieldNames_update() {
  let formAutofillParent = new FormAutofillParent();
  formAutofillParent.init();
  do_register_cleanup(function cleanup() {
    Services.prefs.clearUserPref("browser.formautofill.enabled");
  });

  sinon.stub(formAutofillParent._profileStore, "getAll");
  formAutofillParent._profileStore.getAll.returns([]);

  // The set is empty if there's no profile in the store.
  formAutofillParent._updateSavedFieldNames();
  do_check_eq(Services.ppmm.initialProcessData.autofillSavedFieldNames.size, 0);

  // 2 profiles with 4 valid fields.
  let fakeStorage = [{
    guid: "test-guid-1",
    organization: "Sesame Street",
    "street-address": "123 Sesame Street.",
    tel: "1-345-345-3456",
    email: "",
    timeCreated: 0,
    timeLastUsed: 0,
    timeLastModified: 0,
    timesUsed: 0,
  }, {
    guid: "test-guid-2",
    organization: "Mozilla",
    "street-address": "331 E. Evelyn Avenue",
    tel: "1-650-903-0800",
    country: "US",
    timeCreated: 0,
    timeLastUsed: 0,
    timeLastModified: 0,
    timesUsed: 0,
  }];
  formAutofillParent._profileStore.getAll.returns(fakeStorage);
  formAutofillParent._updateSavedFieldNames();

  let autofillSavedFieldNames = Services.ppmm.initialProcessData.autofillSavedFieldNames;
  do_check_eq(autofillSavedFieldNames.size, 4);
  do_check_eq(autofillSavedFieldNames.has("organization"), true);
  do_check_eq(autofillSavedFieldNames.has("street-address"), true);
  do_check_eq(autofillSavedFieldNames.has("tel"), true);
  do_check_eq(autofillSavedFieldNames.has("email"), false);
  do_check_eq(autofillSavedFieldNames.has("guid"), false);
  do_check_eq(autofillSavedFieldNames.has("timeCreated"), false);
  do_check_eq(autofillSavedFieldNames.has("timeLastUsed"), false);
  do_check_eq(autofillSavedFieldNames.has("timeLastModified"), false);
  do_check_eq(autofillSavedFieldNames.has("timesUsed"), false);
});
