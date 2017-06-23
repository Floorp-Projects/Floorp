"use strict";

const TEST_SELECTORS = {
  selAddresses: "#profiles",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

function waitForAddresses() {
  return new Promise(resolve => {
    Services.cpmm.addMessageListener("FormAutofill:Addresses", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Addresses", getResult);
      // Wait for the next tick for elements to get rendered.
      SimpleTest.executeSoon(resolve.bind(null, result.data));
    });
  });
}

add_task(async function test_manageProfilesInitialState() {
  await BrowserTestUtils.withNewTab({gBrowser, url: MANAGE_PROFILES_DIALOG_URL}, async function(browser) {
    await ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
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

add_task(async function test_cancelManageProfileDialogWithESC() {
  await new Promise(resolve => {
    let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Manage profiles dialog is closed with ESC key");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
    }, {once: true});
  });
});

add_task(async function test_removingSingleAndMultipleProfiles() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);

  let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL, null, DIALOG_SIZE);
  await waitForAddresses();

  let selAddresses = win.document.querySelector(TEST_SELECTORS.selAddresses);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selAddresses.length, 3, "Three addresses");

  EventUtils.synthesizeMouseAtCenter(selAddresses.children[0], {}, win);
  is(btnRemove.disabled, false, "Remove button enabled");
  is(btnEdit.disabled, false, "Edit button enabled");
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await waitForAddresses();
  is(selAddresses.length, 2, "Two addresses left");

  EventUtils.synthesizeMouseAtCenter(selAddresses.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(selAddresses.children[1],
                                     {shiftKey: true}, win);
  is(btnEdit.disabled, true, "Edit button disabled when multi-select");

  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await waitForAddresses();
  is(selAddresses.length, 0, "All addresses are removed");

  win.close();
});

add_task(async function test_profilesDialogWatchesStorageChanges() {
  let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL, null, DIALOG_SIZE);
  await waitForAddresses();

  let selAddresses = win.document.querySelector(TEST_SELECTORS.selAddresses);

  await saveAddress(TEST_ADDRESS_1);
  let addresses = await waitForAddresses();
  is(selAddresses.length, 1, "One address is shown");

  await removeAddresses([addresses[0].guid]);
  await waitForAddresses();
  is(selAddresses.length, 0, "Address is removed");
  win.close();
});
