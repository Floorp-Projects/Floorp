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
