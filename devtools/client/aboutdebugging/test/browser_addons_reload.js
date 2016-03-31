/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

/**
 * Returns a promise that resolves when the given add-on event is fired. The
 * resolved value is an array of arguments passed for the event.
 */
function promiseAddonEvent(event) {
  return new Promise(resolve => {
    let listener = {
      [event]: function(...args) {
        AddonManager.removeAddonListener(listener);
        resolve(args);
      }
    };

    AddonManager.addAddonListener(listener);
  });
}

add_task(function* () {
  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);
  yield installAddon(document, "addons/unpacked/install.rdf",
                     ADDON_NAME, ADDON_NAME);

  // Retrieve the Reload button.
  const names = [...document.querySelectorAll("#addons .target-name")];
  const name = names.filter(element => element.textContent === ADDON_NAME)[0];
  ok(name, "Found " + ADDON_NAME + " add-on in the list");
  const targetElement = name.parentNode.parentNode;
  const reloadButton = targetElement.querySelector(".reload-button");
  ok(reloadButton, "Found its reload button");

  const onDisabled = promiseAddonEvent("onDisabled");
  const onEnabled = promiseAddonEvent("onEnabled");

  const onBootstrapInstallCalled = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, ADDON_NAME, false);
      ok(true, "Add-on was installed: " + ADDON_NAME);
      done();
    }, ADDON_NAME, false);
  });

  reloadButton.click();

  const [disabledAddon] = yield onDisabled;
  ok(disabledAddon.name === ADDON_NAME,
     "Add-on was disabled: " + disabledAddon.name);

  const [enabledAddon] = yield onEnabled;
  ok(enabledAddon.name === ADDON_NAME,
     "Add-on was re-enabled: " + enabledAddon.name);

  yield onBootstrapInstallCalled;

  info("Uninstall addon installed earlier.");
  yield uninstallAddon(document, ADDON_ID, ADDON_NAME);
  yield closeAboutDebugging(tab);
});
