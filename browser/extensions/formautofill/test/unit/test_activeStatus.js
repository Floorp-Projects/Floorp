/*
 * Test for status handling in Form Autofill Parent.
 */

"use strict";

let FormAutofillParent;

add_task(async function setup() {
  ({FormAutofillParent} = ChromeUtils.import("resource://formautofill/FormAutofillParent.jsm", null));
});

add_task(async function test_activeStatus_init() {
  let formAutofillParent = new FormAutofillParent();
  sinon.spy(formAutofillParent, "_updateStatus");

  // Default status is null before initialization
  Assert.equal(formAutofillParent._active, null);
  Assert.equal(Services.ppmm.sharedData.get("FormAutofill:enabled"), undefined);

  await formAutofillParent.init();
  // init shouldn't call updateStatus since that requires storage which will
  // lead to startup time regressions.
  Assert.equal(formAutofillParent._updateStatus.called, false);
  Assert.equal(Services.ppmm.sharedData.get("FormAutofill:enabled"), undefined);

  // Initialize profile storage
  await formAutofillParent.formAutofillStorage.initialize();
  // Upon first initializing profile storage, status should be computed.
  Assert.equal(formAutofillParent._updateStatus.called, true);
  Assert.equal(Services.ppmm.sharedData.get("FormAutofill:enabled"), false);

  formAutofillParent._uninit();
});

add_task(async function test_activeStatus_observe() {
  let formAutofillParent = new FormAutofillParent();
  sinon.stub(formAutofillParent, "_computeStatus");
  sinon.spy(formAutofillParent, "_onStatusChanged");

  // _active = _computeStatus() => No need to trigger _onStatusChanged
  formAutofillParent._active = true;
  formAutofillParent._computeStatus.returns(true);
  formAutofillParent.observe(null, "nsPref:changed", "extensions.formautofill.addresses.enabled");
  formAutofillParent.observe(null, "nsPref:changed", "extensions.formautofill.creditCards.enabled");
  Assert.equal(formAutofillParent._onStatusChanged.called, false);

  // _active != _computeStatus() => Need to trigger _onStatusChanged
  formAutofillParent._computeStatus.returns(false);
  formAutofillParent._onStatusChanged.resetHistory();
  formAutofillParent.observe(null, "nsPref:changed", "extensions.formautofill.addresses.enabled");
  formAutofillParent.observe(null, "nsPref:changed", "extensions.formautofill.creditCards.enabled");
  Assert.equal(formAutofillParent._onStatusChanged.called, true);

  // profile changed => Need to trigger _onStatusChanged
  await Promise.all(["add", "update", "remove", "reconcile"].map(async event => {
    formAutofillParent._computeStatus.returns(!formAutofillParent._active);
    formAutofillParent._onStatusChanged.resetHistory();
    await formAutofillParent.observe(null, "formautofill-storage-changed", event);
    Assert.equal(formAutofillParent._onStatusChanged.called, true);
  }));

  // profile metadata updated => No need to trigger _onStatusChanged
  formAutofillParent._computeStatus.returns(!formAutofillParent._active);
  formAutofillParent._onStatusChanged.resetHistory();
  await formAutofillParent.observe(null, "formautofill-storage-changed", "notifyUsed");
  Assert.equal(formAutofillParent._onStatusChanged.called, false);
});

add_task(async function test_activeStatus_computeStatus() {
  let formAutofillParent = new FormAutofillParent();
  registerCleanupFunction(function cleanup() {
    Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
    Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
  });

  sinon.stub(formAutofillParent.formAutofillStorage.addresses, "getSavedFieldNames");
  formAutofillParent.formAutofillStorage.addresses.getSavedFieldNames.returns(new Set());
  sinon.stub(formAutofillParent.formAutofillStorage.creditCards, "getSavedFieldNames");
  formAutofillParent.formAutofillStorage.creditCards.getSavedFieldNames.returns(new Set());

  // pref is enabled and profile is empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", true);
  Assert.equal(formAutofillParent._computeStatus(), false);

  // pref is disabled and profile is empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", false);
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", false);
  Assert.equal(formAutofillParent._computeStatus(), false);

  formAutofillParent.formAutofillStorage.addresses.getSavedFieldNames.returns(new Set(["given-name"]));
  formAutofillParent.observe(null, "formautofill-storage-changed", "add");

  // pref is enabled and profile is not empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Assert.equal(formAutofillParent._computeStatus(), true);

  // pref is partial enabled and profile is not empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", false);
  Assert.equal(formAutofillParent._computeStatus(), true);
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", false);
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", true);
  Assert.equal(formAutofillParent._computeStatus(), true);

  // pref is disabled and profile is not empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", false);
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", false);
  Assert.equal(formAutofillParent._computeStatus(), false);
});
