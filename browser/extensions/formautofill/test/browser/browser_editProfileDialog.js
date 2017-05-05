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
      EventUtils.synthesizeKey("VK_RETURN", {}, win);
    }, {once: true});
  });
  let profiles = yield getProfiles();

  is(profiles.length, 1, "only one profile is in storage");
  is(profiles[0].organization, TEST_PROFILE_1.organization, "match organization");
  is(profiles[0]["street-address"], TEST_PROFILE_1["street-address"], "match street-address");
  is(profiles[0]["address-level2"], TEST_PROFILE_1["address-level2"], "match address-level2");
  is(profiles[0]["address-level1"], TEST_PROFILE_1["address-level1"], "match address-level1");
  is(profiles[0]["postal-code"], TEST_PROFILE_1["postal-code"], "match postal-code");
  is(profiles[0].country, TEST_PROFILE_1.country, "match country");
  is(profiles[0].email, TEST_PROFILE_1.email, "match email");
  is(profiles[0].tel, TEST_PROFILE_1.tel, "match tel");
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
  is(profiles[0].organization, TEST_PROFILE_1.organization + "test", "organization changed");
  yield removeProfiles([profiles[0].guid]);

  profiles = yield getProfiles();
  is(profiles.length, 0, "Profile storage is empty");
});
