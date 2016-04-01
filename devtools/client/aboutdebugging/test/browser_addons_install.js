/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

add_task(function* () {
  let { tab, document } = yield openAboutDebugging("addons");

  yield installAddon(document, "addons/unpacked/install.rdf", "test-devtools");

  // Check that the addon appears in the UI
  let names = [...document.querySelectorAll("#addons .target-name")];
  names = names.map(element => element.textContent);
  ok(names.includes(ADDON_NAME),
    "The addon name appears in the list of addons: " + names);

  // Now uninstall this addon
  yield uninstallAddon(ADDON_ID);

  // Ensure that the UI removes the addon from the list
  names = [...document.querySelectorAll("#addons .target-name")];
  names = names.map(element => element.textContent);
  ok(!names.includes(ADDON_NAME),
    "After uninstall, the addon name disappears from the list of addons: "
    + names);

  yield closeAboutDebugging(tab);
});

add_task(function* () {
  let { tab, document } = yield openAboutDebugging("addons");

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
