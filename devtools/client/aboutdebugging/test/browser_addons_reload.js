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
      [event]: function (...args) {
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
  ok(name, `Found ${ADDON_NAME} add-on in the list`);
  const targetElement = name.parentNode.parentNode;
  const reloadButton = targetElement.querySelector(".reload-button");
  ok(reloadButton, "Found its reload button");

  const onInstalled = promiseAddonEvent("onInstalled");

  const onBootstrapInstallCalled = new Promise(done => {
    Services.obs.addObserver(function listener() {
      Services.obs.removeObserver(listener, ADDON_NAME, false);
      info("Add-on was re-installed: " + ADDON_NAME);
      done();
    }, ADDON_NAME, false);
  });

  reloadButton.click();

  const [reloadedAddon] = yield onInstalled;
  is(reloadedAddon.name, ADDON_NAME,
     "Add-on was reloaded: " + reloadedAddon.name);

  yield onBootstrapInstallCalled;

  info("Uninstall addon installed earlier.");
  const onUninstalled = promiseAddonEvent("onUninstalled");
  reloadedAddon.uninstall();
  const [uninstalledAddon] = yield onUninstalled;
  is(uninstalledAddon.id, ADDON_ID,
     "Add-on was uninstalled: " + uninstalledAddon.id);

  yield closeAboutDebugging(tab);
});
