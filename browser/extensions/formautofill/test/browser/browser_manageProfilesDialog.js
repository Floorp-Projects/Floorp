"use strict";

const TEST_SELECTORS = {
  selAddresses: "#profiles",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

function waitForAddresses() {
  return new Promise(resolve => {
    Services.cpmm.addMessageListener("FormAutofill:Addresses", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Addresses", getResult);
      // Wait for the next tick for elements to get rendered.
      SimpleTest.executeSoon(resolve.bind(null, result.data));
    });
  });
}

registerCleanupFunction(function* () {
  let addresses = yield getAddresses();
  if (addresses.length) {
    yield removeAddresses(addresses.map(address => address.guid));
  }
});

add_task(function* test_manageProfilesInitialState() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: MANAGE_PROFILES_DIALOG_URL}, function* (browser) {
    yield ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      let selAddresses = content.document.querySelector(args.selAddresses);
      let btnRemove = content.document.querySelector(args.btnRemove);
      let btnEdit = content.document.querySelector(args.btnEdit);
      let btnAdd = content.document.querySelector(args.btnAdd);

      is(selAddresses.length, 0, "No address");
      is(btnAdd.disabled, false, "Add button enabled");
      is(btnRemove.disabled, true, "Remove button disabled");
      is(btnEdit.disabled, true, "Edit button disabled");
    });
  });
});

add_task(function* test_removingSingleAndMultipleProfiles() {
  yield saveAddress(TEST_ADDRESS_1);
  yield saveAddress(TEST_ADDRESS_2);
  yield saveAddress(TEST_ADDRESS_3);

  let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL);
  yield waitForAddresses();

  let selAddresses = win.document.querySelector(TEST_SELECTORS.selAddresses);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selAddresses.length, 3, "Three addresses");

  EventUtils.synthesizeMouseAtCenter(selAddresses.children[0], {}, win);
  is(btnRemove.disabled, false, "Remove button enabled");
  is(btnEdit.disabled, false, "Edit button enabled");
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  yield waitForAddresses();
  is(selAddresses.length, 2, "Two addresses left");

  EventUtils.synthesizeMouseAtCenter(selAddresses.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(selAddresses.children[1],
                                     {shiftKey: true}, win);
  is(btnEdit.disabled, true, "Edit button disabled when multi-select");

  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  yield waitForAddresses();
  is(selAddresses.length, 0, "All addresses are removed");

  win.close();
});

add_task(function* test_profilesDialogWatchesStorageChanges() {
  let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL);
  yield waitForAddresses();

  let selAddresses = win.document.querySelector(TEST_SELECTORS.selAddresses);

  yield saveAddress(TEST_ADDRESS_1);
  let addresses = yield waitForAddresses();
  is(selAddresses.length, 1, "One address is shown");

  yield removeAddresses([addresses[0].guid]);
  yield waitForAddresses();
  is(selAddresses.length, 0, "Address is removed");
  win.close();
});
