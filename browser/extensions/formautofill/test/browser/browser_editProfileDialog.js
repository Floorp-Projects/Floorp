"use strict";

add_task(async function test_cancelEditProfileDialog() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_PROFILE_DIALOG_URL, null, null, null);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit profile dialog is closed");
        resolve();
      }, {once: true});
      win.document.querySelector("#cancel").click();
    }, {once: true});
  });
});

add_task(async function test_cancelEditProfileDialogWithESC() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_PROFILE_DIALOG_URL);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit profile dialog is closed with ESC key");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
    }, {once: true});
  });
});

add_task(async function test_saveAddress() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_PROFILE_DIALOG_URL, null, null, null);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit profile dialog is closed");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["given-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["additional-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["family-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1.organization, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["street-address"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["address-level2"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["address-level1"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1["postal-code"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1.country, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1.email, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_ADDRESS_1.tel, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      info("saving profile");
      EventUtils.synthesizeKey("VK_RETURN", {}, win);
    }, {once: true});
  });
  let addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(Object.keys(TEST_ADDRESS_1).length, 11, "Sanity check number of properties");
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
});

add_task(async function test_editProfile() {
  let addresses = await getAddresses();
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_PROFILE_DIALOG_URL, null, null, addresses[0]);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit profile dialog is closed");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    }, {once: true});
  });
  addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(addresses[0]["given-name"], TEST_ADDRESS_1["given-name"] + "test", "given-name changed");
  await removeAddresses([addresses[0].guid]);

  addresses = await getAddresses();
  is(addresses.length, 0, "Address storage is empty");
});
