/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that system and hidden addons are only displayed when the showSystemAddons
// preferences is true.

const SYSTEM_ADDON = createAddonData({
  id: "system",
  name: "System Addon",
  isSystem: true,
  hidden: true,
});
const HIDDEN_ADDON = createAddonData({
  id: "hidden",
  name: "Hidden Addon",
  isSystem: false,
  hidden: true,
});
const NORMAL_ADDON = createAddonData({
  id: "normal",
  name: "Normal Addon",
  isSystem: false,
  hidden: false,
});

add_task(async function testShowSystemAddonsTrue() {
  info("Test with showHiddenAddons set to true");
  await testAddonsDisplay(true);

  info("Test with showHiddenAddons set to false");
  await testAddonsDisplay(false);
});

async function testAddonsDisplay(showHidden) {
  const thisFirefoxClient = setupThisFirefoxMock();
  thisFirefoxClient.listAddons = () => [
    SYSTEM_ADDON,
    HIDDEN_ADDON,
    NORMAL_ADDON,
  ];

  info("Set showHiddenAddons to " + showHidden);
  await pushPref("devtools.aboutdebugging.showHiddenAddons", showHidden);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const hasSystemAddon = !!findDebugTargetByText("System Addon", document);
  const hasHiddenAddon = !!findDebugTargetByText("Hidden Addon", document);
  const hasInstalledAddon = !!findDebugTargetByText("Normal Addon", document);
  is(
    hasSystemAddon,
    showHidden,
    "System addon display is correct when showHiddenAddons is " + showHidden
  );
  is(
    hasHiddenAddon,
    showHidden,
    "Hidden addon display is correct when showHiddenAddons is " + showHidden
  );
  ok(hasInstalledAddon, "Installed addon is always displayed");

  await removeTab(tab);
}

// Create a basic mock for this-firefox client, and setup a runtime-client-factory mock
// to return our mock client when needed.
function setupThisFirefoxMock() {
  const runtimeClientFactoryMock = createRuntimeClientFactoryMock();
  const thisFirefoxClient = createThisFirefoxClientMock();
  runtimeClientFactoryMock.createClientForRuntime = runtime => {
    const {
      RUNTIMES,
    } = require("devtools/client/aboutdebugging/src/constants");
    if (runtime.id === RUNTIMES.THIS_FIREFOX) {
      return thisFirefoxClient;
    }
    throw new Error("Unexpected runtime id " + runtime.id);
  };

  info("Enable mocks");
  enableRuntimeClientFactoryMock(runtimeClientFactoryMock);
  registerCleanupFunction(() => {
    disableRuntimeClientFactoryMock();
  });

  return thisFirefoxClient;
}

// Create basic addon data as the DebuggerClient would return it (debuggable and non
// temporary).
function createAddonData({ id, name, isSystem, hidden }) {
  return {
    actor: `actorid-${id}`,
    hidden,
    iconURL: `moz-extension://${id}/icon-url.png`,
    id,
    manifestURL: `moz-extension://${id}/manifest-url.json`,
    name,
    isSystem,
    temporarilyInstalled: false,
    debuggable: true,
  };
}
