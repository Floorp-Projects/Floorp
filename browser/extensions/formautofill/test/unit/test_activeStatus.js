/*
 * Test for status handling in Form Autofill Parent.
 */

"use strict";

let FormAutofillStatus;

add_setup(async () => {
  ({ FormAutofillStatus } = ChromeUtils.import(
    "resource://autofill/FormAutofillParent.jsm"
  ));
});

add_task(async function test_activeStatus_init() {
  sinon.spy(FormAutofillStatus, "updateStatus");

  // Default status is null before initialization
  Assert.equal(FormAutofillStatus._active, null);
  Assert.equal(Services.ppmm.sharedData.get("FormAutofill:enabled"), undefined);

  FormAutofillStatus.init();
  // init shouldn't call updateStatus since that requires storage which will
  // lead to startup time regressions.
  Assert.equal(FormAutofillStatus.updateStatus.called, false);
  Assert.equal(Services.ppmm.sharedData.get("FormAutofill:enabled"), undefined);

  // Initialize profile storage
  await FormAutofillStatus.formAutofillStorage.initialize();
  await FormAutofillStatus.updateSavedFieldNames();
  // Upon first initializing profile storage, status should be computed.
  Assert.equal(FormAutofillStatus.updateStatus.called, true);
  Assert.equal(Services.ppmm.sharedData.get("FormAutofill:enabled"), false);

  FormAutofillStatus.uninit();
});

add_task(async function test_activeStatus_observe() {
  FormAutofillStatus.init();
  sinon.stub(FormAutofillStatus, "computeStatus");
  sinon.spy(FormAutofillStatus, "onStatusChanged");

  // _active = _computeStatus() => No need to trigger _onStatusChanged
  FormAutofillStatus._active = true;
  FormAutofillStatus.computeStatus.returns(true);
  FormAutofillStatus.observe(
    null,
    "nsPref:changed",
    "extensions.formautofill.addresses.enabled"
  );
  FormAutofillStatus.observe(
    null,
    "nsPref:changed",
    "extensions.formautofill.creditCards.enabled"
  );
  Assert.equal(FormAutofillStatus.onStatusChanged.called, false);

  // _active != computeStatus() => Need to trigger onStatusChanged
  FormAutofillStatus.computeStatus.returns(false);
  FormAutofillStatus.onStatusChanged.resetHistory();
  FormAutofillStatus.observe(
    null,
    "nsPref:changed",
    "extensions.formautofill.addresses.enabled"
  );
  FormAutofillStatus.observe(
    null,
    "nsPref:changed",
    "extensions.formautofill.creditCards.enabled"
  );
  Assert.equal(FormAutofillStatus.onStatusChanged.called, true);

  // profile changed => Need to trigger _onStatusChanged
  await Promise.all(
    ["add", "update", "remove", "reconcile"].map(async event => {
      FormAutofillStatus.computeStatus.returns(!FormAutofillStatus._active);
      FormAutofillStatus.onStatusChanged.resetHistory();
      await FormAutofillStatus.observe(
        null,
        "formautofill-storage-changed",
        event
      );
      Assert.equal(FormAutofillStatus.onStatusChanged.called, true);
    })
  );

  // profile metadata updated => No need to trigger onStatusChanged
  FormAutofillStatus.computeStatus.returns(!FormAutofillStatus._active);
  FormAutofillStatus.onStatusChanged.resetHistory();
  await FormAutofillStatus.observe(
    null,
    "formautofill-storage-changed",
    "notifyUsed"
  );
  Assert.equal(FormAutofillStatus.onStatusChanged.called, false);

  FormAutofillStatus.computeStatus.restore();
});

add_task(async function test_activeStatus_computeStatus() {
  registerCleanupFunction(function cleanup() {
    Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
    Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
  });

  sinon.stub(
    FormAutofillStatus.formAutofillStorage.addresses,
    "getSavedFieldNames"
  );
  FormAutofillStatus.formAutofillStorage.addresses.getSavedFieldNames.returns(
    Promise.resolve(new Set())
  );
  sinon.stub(
    FormAutofillStatus.formAutofillStorage.creditCards,
    "getSavedFieldNames"
  );
  FormAutofillStatus.formAutofillStorage.creditCards.getSavedFieldNames.returns(
    Promise.resolve(new Set())
  );

  // pref is enabled and profile is empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    true
  );
  Assert.equal(FormAutofillStatus.computeStatus(), false);

  // pref is disabled and profile is empty.
  Services.prefs.setBoolPref(
    "extensions.formautofill.addresses.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    false
  );
  Assert.equal(FormAutofillStatus.computeStatus(), false);

  FormAutofillStatus.formAutofillStorage.addresses.getSavedFieldNames.returns(
    Promise.resolve(new Set(["given-name"]))
  );
  await FormAutofillStatus.observe(null, "formautofill-storage-changed", "add");

  // pref is enabled and profile is not empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Assert.equal(FormAutofillStatus.computeStatus(), true);

  // pref is partial enabled and profile is not empty.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    false
  );
  Assert.equal(FormAutofillStatus.computeStatus(), true);
  Services.prefs.setBoolPref(
    "extensions.formautofill.addresses.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    true
  );
  Assert.equal(FormAutofillStatus.computeStatus(), true);

  // pref is disabled and profile is not empty.
  Services.prefs.setBoolPref(
    "extensions.formautofill.addresses.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    false
  );
  Assert.equal(FormAutofillStatus.computeStatus(), false);
});
