/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var {AddonManager} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

add_task(function *() {
  let { tab, document } = yield openAboutDebugging("addons");

  // Mock the file picker to select a test addon
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(null);
  let file = get_supports_file("addons/unpacked/install.rdf");
  MockFilePicker.returnFiles = [file.file];

  // Wait for a message sent by the addon's bootstrap.js file
  let promise = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, "test-devtools", false);
      ok(true, "Addon installed and running its bootstrap.js file");
      done();
    }, "test-devtools", false);
  });
  // Trigger the file picker by clicking on the button
  document.getElementById("load-addon-from-file").click();

  // Wait for the addon execution
  yield promise;

  // Check that the addon appears in the UI
  let names = [...document.querySelectorAll("#addons .target-name")];
  names = names.map(element => element.textContent);
  ok(names.includes(ADDON_NAME), "The addon name appears in the list of addons: " + names);

  // Now uninstall this addon
  yield new Promise(done => {
    AddonManager.getAddonByID(ADDON_ID, addon => {
      let listener = {
        onUninstalled: function(aUninstalledAddon) {
          if (aUninstalledAddon != addon) {
            return;
          }
          AddonManager.removeAddonListener(listener);
          done();
        }
      };
      AddonManager.addAddonListener(listener);
      addon.uninstall();
    });
  });

  // Ensure that the UI removes the addon from the list
  names = [...document.querySelectorAll("#addons .target-name")];
  names = names.map(element => element.textContent);
  ok(!names.includes(ADDON_NAME), "After uninstall, the addon name disappears from the list of addons: " + names);

  yield closeAboutDebugging(tab);
});
