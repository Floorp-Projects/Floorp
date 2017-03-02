/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

add_task(function* () {
  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  // Install this add-on, and verify that it appears in the about:debugging UI
  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  // Install the add-on, and verify that it disappears in the about:debugging UI
  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  yield closeAboutDebugging(tab);
});

add_task(function* () {
  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  // Start an observer that looks for the install error before
  // actually doing the install
  let top = document.querySelector(".addons-top");
  let promise = waitForMutation(top, { childList: true });

  // Mock the file picker to select a test addon
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(null);
  let file = getSupportsFile("addons/bad/manifest.json");
  MockFilePicker.returnFiles = [file.file];

  // Trigger the file picker by clicking on the button
  document.getElementById("load-addon-from-file").click();

  // Now wait for the install error to appear.
  yield promise;

  // And check that it really is there.
  let err = document.querySelector(".addons-install-error");
  isnot(err, null, "Addon install error message appeared");

  yield closeAboutDebugging(tab);
});
