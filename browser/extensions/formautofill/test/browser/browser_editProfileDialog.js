"use strict";

registerCleanupFunction(function* () {
  let addresses = yield getAddresses();
  if (addresses.length) {
    yield removeAddresses(addresses.map(address => address.guid));
  }
});

add_task(function* test_cancelEditProfileDialog() {
  yield new Promise(resolve => {
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

add_task(function* test_saveAddress() {
  yield new Promise(resolve => {
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
  let addresses = yield getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(Object.keys(TEST_ADDRESS_1).length, 11, "Sanity check number of properties");
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
});

add_task(function* test_editProfile() {
  let addresses = yield getAddresses();
  yield new Promise(resolve => {
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
  addresses = yield getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(addresses[0]["given-name"], TEST_ADDRESS_1["given-name"] + "test", "given-name changed");
  yield removeAddresses([addresses[0].guid]);

  addresses = yield getAddresses();
  is(addresses.length, 0, "Address storage is empty");
});
