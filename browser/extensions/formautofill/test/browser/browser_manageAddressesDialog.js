"use strict";

const TEST_SELECTORS = {
  selRecords: "#addresses",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_task(async function test_manageAddressesInitialState() {
  await BrowserTestUtils.withNewTab({gBrowser, url: MANAGE_ADDRESSES_DIALOG_URL}, async function(browser) {
    await ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      let selRecords = content.document.querySelector(args.selRecords);
      let btnRemove = content.document.querySelector(args.btnRemove);
      let btnEdit = content.document.querySelector(args.btnEdit);
      let btnAdd = content.document.querySelector(args.btnAdd);

      is(selRecords.length, 0, "No address");
      is(btnAdd.disabled, false, "Add button enabled");
      is(btnRemove.disabled, true, "Remove button disabled");
      is(btnEdit.disabled, true, "Edit button disabled");
    });
  });
});

add_task(async function test_cancelManageAddressDialogWithESC() {
  let win = window.openDialog(MANAGE_ADDRESSES_DIALOG_URL);
  await waitForFocusAndFormReady(win);
  let unloadPromise = BrowserTestUtils.waitForEvent(win, "unload");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  await unloadPromise;
  ok(true, "Manage addresses dialog is closed with ESC key");
});

add_task(async function test_removingSingleAndMultipleAddresses() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);

  let win = window.openDialog(MANAGE_ADDRESSES_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selRecords.length, 3, "Three addresses");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  is(btnRemove.disabled, false, "Remove button enabled");
  is(btnEdit.disabled, false, "Edit button enabled");
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 2, "Two addresses left");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(selRecords.children[1],
                                     {shiftKey: true}, win);
  is(btnEdit.disabled, true, "Edit button disabled when multi-select");

  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "All addresses are removed");

  win.close();
});

add_task(async function test_removingAdressViaKeyboardDelete() {
  await saveAddress(TEST_ADDRESS_1);
  let win = window.openDialog(MANAGE_ADDRESSES_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  is(selRecords.length, 1, "One address");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeKey("VK_DELETE", {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "No addresses left");

  win.close();
});

add_task(async function test_addressesDialogWatchesStorageChanges() {
  let win = window.openDialog(MANAGE_ADDRESSES_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  await saveAddress(TEST_ADDRESS_1);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(selRecords.length, 1, "One address is shown");

  await removeAddresses([selRecords.options[0].value]);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(selRecords.length, 0, "Address is removed");
  win.close();
});
