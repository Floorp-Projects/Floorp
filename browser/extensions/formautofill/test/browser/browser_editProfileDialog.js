"use strict";

registerCleanupFunction(function* () {
  let profiles = yield getProfiles();
  if (profiles.length) {
    yield removeProfiles(profiles.map(profile => profile.guid));
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

add_task(function* test_saveProfile() {
  yield new Promise(resolve => {
    let win = window.openDialog(EDIT_PROFILE_DIALOG_URL, null, null, null);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit profile dialog is closed");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["given-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["additional-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["family-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1.organization, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["street-address"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["address-level2"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["address-level1"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1["postal-code"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1.country, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1.email, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_PROFILE_1.tel, {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      info("saving profile");
      EventUtils.synthesizeKey("VK_RETURN", {}, win);
    }, {once: true});
  });
  let profiles = yield getProfiles();

  is(profiles.length, 1, "only one profile is in storage");
  is(Object.keys(TEST_PROFILE_1).length, 11, "Sanity check number of properties");
  for (let [fieldName, fieldValue] of Object.entries(TEST_PROFILE_1)) {
    is(profiles[0][fieldName], fieldValue, "check " + fieldName);
  }
});

add_task(function* test_editProfile() {
  let profiles = yield getProfiles();
  yield new Promise(resolve => {
    let win = window.openDialog(EDIT_PROFILE_DIALOG_URL, null, null, profiles[0]);
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
  profiles = yield getProfiles();

  is(profiles.length, 1, "only one profile is in storage");
  is(profiles[0]["given-name"], TEST_PROFILE_1["given-name"] + "test", "given-name changed");
  yield removeProfiles([profiles[0].guid]);

  profiles = yield getProfiles();
  is(profiles.length, 0, "Profile storage is empty");
});
