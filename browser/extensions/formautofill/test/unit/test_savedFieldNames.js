/*
 * Test for keeping the valid fields information in sharedData.
 */

"use strict";

let FormAutofillStatus;

add_setup(async () => {
  ({ FormAutofillStatus } = ChromeUtils.import(
    "resource://autofill/FormAutofillParent.jsm"
  ));
});

add_task(async function test_profileSavedFieldNames_init() {
  FormAutofillStatus.init();
  sinon.stub(FormAutofillStatus, "updateSavedFieldNames");

  await FormAutofillStatus.formAutofillStorage.initialize();
  Assert.equal(FormAutofillStatus.updateSavedFieldNames.called, true);

  FormAutofillStatus.uninit();
});

add_task(async function test_profileSavedFieldNames_observe() {
  FormAutofillStatus.init();

  // profile changed => Need to trigger updateValidFields
  ["add", "update", "remove", "reconcile", "removeAll"].forEach(event => {
    FormAutofillStatus.observe(null, "formautofill-storage-changed", event);
    Assert.equal(FormAutofillStatus.updateSavedFieldNames.called, true);
  });

  // profile metadata updated => no need to trigger updateValidFields
  FormAutofillStatus.updateSavedFieldNames.resetHistory();
  FormAutofillStatus.observe(
    null,
    "formautofill-storage-changed",
    "notifyUsed"
  );
  Assert.equal(FormAutofillStatus.updateSavedFieldNames.called, false);
  FormAutofillStatus.updateSavedFieldNames.restore();
});

add_task(async function test_profileSavedFieldNames_update() {
  registerCleanupFunction(function cleanup() {
    Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
  });

  Object.defineProperty(
    FormAutofillStatus.formAutofillStorage.addresses,
    "_data",
    { writable: true }
  );

  FormAutofillStatus.formAutofillStorage.addresses._data = [];

  // The set is empty if there's no profile in the store.
  await FormAutofillStatus.updateSavedFieldNames();
  Assert.equal(
    Services.ppmm.sharedData.get("FormAutofill:savedFieldNames").size,
    0
  );

  // 2 profiles with 4 valid fields.
  FormAutofillStatus.formAutofillStorage.addresses._data = [
    {
      guid: "test-guid-1",
      organization: "Sesame Street",
      "street-address": "123 Sesame Street.",
      tel: "1-345-345-3456",
      email: "",
      timeCreated: 0,
      timeLastUsed: 0,
      timeLastModified: 0,
      timesUsed: 0,
    },
    {
      guid: "test-guid-2",
      organization: "Mozilla",
      "street-address": "331 E. Evelyn Avenue",
      tel: "1-650-903-0800",
      country: "US",
      timeCreated: 0,
      timeLastUsed: 0,
      timeLastModified: 0,
      timesUsed: 0,
    },
  ];

  await FormAutofillStatus.updateSavedFieldNames();

  let autofillSavedFieldNames = Services.ppmm.sharedData.get(
    "FormAutofill:savedFieldNames"
  );
  Assert.equal(autofillSavedFieldNames.size, 4);
  Assert.equal(autofillSavedFieldNames.has("organization"), true);
  Assert.equal(autofillSavedFieldNames.has("street-address"), true);
  Assert.equal(autofillSavedFieldNames.has("tel"), true);
  Assert.equal(autofillSavedFieldNames.has("email"), false);
  Assert.equal(autofillSavedFieldNames.has("guid"), false);
  Assert.equal(autofillSavedFieldNames.has("timeCreated"), false);
  Assert.equal(autofillSavedFieldNames.has("timeLastUsed"), false);
  Assert.equal(autofillSavedFieldNames.has("timeLastModified"), false);
  Assert.equal(autofillSavedFieldNames.has("timesUsed"), false);
});
