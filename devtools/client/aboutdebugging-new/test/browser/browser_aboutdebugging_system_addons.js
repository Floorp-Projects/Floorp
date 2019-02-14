/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that system addons are only displayed when the showSystemAddons preference is
// true.

const SYSTEM_ADDON =
  createAddonData({ id: "system", name: "System Addon", isSystem: true });
const INSTALLED_ADDON =
  createAddonData({ id: "installed", name: "Installed Addon", isSystem: false });

add_task(async function testShowSystemAddonsFalse() {
  const thisFirefoxClient = setupThisFirefoxMock();
  thisFirefoxClient.listAddons = () => ([SYSTEM_ADDON, INSTALLED_ADDON]);

  info("Hide system addons in aboutdebugging via preference");
  await pushPref("devtools.aboutdebugging.showSystemAddons", false);

  const { document, tab } = await openAboutDebugging();

  const hasSystemAddon = !!findDebugTargetByText("System Addon", document);
  const hasInstalledAddon = !!findDebugTargetByText("Installed Addon", document);
  ok(!hasSystemAddon, "System addon is hidden when system addon pref is false");
  ok(hasInstalledAddon, "Installed addon is displayed when system addon pref is false");

  await removeTab(tab);
});

add_task(async function testShowSystemAddonsTrue() {
  const thisFirefoxClient = setupThisFirefoxMock();
  thisFirefoxClient.listAddons = () => ([SYSTEM_ADDON, INSTALLED_ADDON]);

  info("Show system addons in aboutdebugging via preference");
  await pushPref("devtools.aboutdebugging.showSystemAddons", true);

  const { document, tab } = await openAboutDebugging();
  const hasSystemAddon = !!findDebugTargetByText("System Addon", document);
  const hasInstalledAddon = !!findDebugTargetByText("Installed Addon", document);
  ok(hasSystemAddon, "System addon is displayed when system addon pref is true");
  ok(hasInstalledAddon, "Installed addon is displayed when system addon pref is true");

  await removeTab(tab);
});

// Create a basic mock for this-firefox client, and setup a runtime-client-factory mock
// to return our mock client when needed.
function setupThisFirefoxMock() {
  const runtimeClientFactoryMock = createRuntimeClientFactoryMock();
  const thisFirefoxClient = createThisFirefoxClientMock();
  runtimeClientFactoryMock.createClientForRuntime = runtime => {
    const { RUNTIMES } = require("devtools/client/aboutdebugging-new/src/constants");
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
function createAddonData({ id, name, isSystem }) {
  return {
    actor: `actorid-${id}`,
    iconURL: `moz-extension://${id}/icon-url.png`,
    id,
    manifestURL: `moz-extension://${id}/manifest-url.json`,
    name,
    isSystem,
    temporarilyInstalled: false,
    debuggable: true,
  };
}
