/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.sys.mjs",
  WindowsLaunchOnLogin: "resource://gre/modules/WindowsLaunchOnLogin.sys.mjs",
});

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_task(async function test_check_checkbox() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    launchOnLoginCheckbox.click();
    ok(launchOnLoginCheckbox.checked, "Autostart checkbox checked");

    ok(
      wrk.hasValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName()),
      "Key exists"
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

add_task(async function test_uncheck_checkbox() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    launchOnLoginCheckbox.click();
    ok(!launchOnLoginCheckbox.checked, "Autostart checkbox unchecked");

    ok(
      !wrk.hasValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName()),
      "Autostart registry key does not exist"
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

add_task(async function create_external_regkey() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Create registry key without using settings pane to check if
    // this is reflected in the settings
    let autostartPath =
      WindowsLaunchOnLogin.quoteString(
        Services.dirsvc.get("XREExeF", Ci.nsIFile).path
      ) + " -os-autostart";
    wrk.writeStringValue(
      WindowsLaunchOnLogin.getLaunchOnLoginRegistryName(),
      autostartPath
    );

    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    ok(
      launchOnLoginCheckbox.checked,
      "Autostart checkbox automatically checked"
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

add_task(async function delete_external_regkey() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Delete registry key without using settings pane to check if
    // this is reflected in the settings
    wrk.removeValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName());

    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    ok(
      !launchOnLoginCheckbox.checked,
      "Launch on login checkbox automatically unchecked"
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

registerCleanupFunction(async function () {
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    let registryName = WindowsLaunchOnLogin.getLaunchOnLoginRegistryName();
    if (wrk.hasValue(registryName)) {
      wrk.removeValue(registryName);
    }
  });
});
