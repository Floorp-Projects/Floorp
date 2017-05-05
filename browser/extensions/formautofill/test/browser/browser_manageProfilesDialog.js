"use strict";

const TEST_SELECTORS = {
  selProfiles: "#profiles",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

function waitForProfiles() {
  return new Promise(resolve => {
    Services.cpmm.addMessageListener("FormAutofill:Profiles", function getResult(result) {
      Services.cpmm.removeMessageListener("FormAutofill:Profiles", getResult);
      // Wait for the next tick for elements to get rendered.
      SimpleTest.executeSoon(resolve.bind(null, result.data));
    });
  });
}

registerCleanupFunction(function* () {
  let profiles = yield getProfiles();
  if (profiles.length) {
    yield removeProfiles(profiles.map(profile => profile.guid));
  }
});

add_task(function* test_manageProfilesInitialState() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: MANAGE_PROFILES_DIALOG_URL}, function* (browser) {
    yield ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      let selProfiles = content.document.querySelector(args.selProfiles);
      let btnRemove = content.document.querySelector(args.btnRemove);
      let btnEdit = content.document.querySelector(args.btnEdit);
      let btnAdd = content.document.querySelector(args.btnAdd);

      is(selProfiles.length, 0, "No profile");
      is(btnAdd.disabled, false, "Add button enabled");
      is(btnRemove.disabled, true, "Remove button disabled");
      is(btnEdit.disabled, true, "Edit button disabled");
    });
  });
});

add_task(function* test_removingSingleAndMultipleProfiles() {
  yield saveProfile(TEST_PROFILE_1);
  yield saveProfile(TEST_PROFILE_2);
  yield saveProfile(TEST_PROFILE_3);

  let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL);
  yield waitForProfiles();

  let selProfiles = win.document.querySelector(TEST_SELECTORS.selProfiles);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selProfiles.length, 3, "Three profiles");

  EventUtils.synthesizeMouseAtCenter(selProfiles.children[0], {}, win);
  is(btnRemove.disabled, false, "Remove button enabled");
  is(btnEdit.disabled, false, "Edit button enabled");
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  yield waitForProfiles();
  is(selProfiles.length, 2, "Two profiles left");

  EventUtils.synthesizeMouseAtCenter(selProfiles.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(selProfiles.children[1],
                                     {shiftKey: true}, win);
  is(btnEdit.disabled, true, "Edit button disabled when multi-select");

  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  yield waitForProfiles();
  is(selProfiles.length, 0, "All profiles are removed");

  win.close();
});

add_task(function* test_profilesDialogWatchesStorageChanges() {
  let win = window.openDialog(MANAGE_PROFILES_DIALOG_URL);
  yield waitForProfiles();

  let selProfiles = win.document.querySelector(TEST_SELECTORS.selProfiles);

  yield saveProfile(TEST_PROFILE_1);
  let profiles = yield waitForProfiles();
  is(selProfiles.length, 1, "One profile is shown");

  yield removeProfiles([profiles[0].guid]);
  yield waitForProfiles();
  is(selProfiles.length, 0, "Profile is removed");
  win.close();
});
