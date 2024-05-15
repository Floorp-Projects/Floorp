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

add_task(async function test_check_uncheck_checkbox() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  // Open preferences to general pane
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
  launchOnLoginCheckbox.click();
  ok(launchOnLoginCheckbox.checked, "Autostart checkbox checked");

  ok(
    await WindowsLaunchOnLogin.getLaunchOnLoginEnabledMSIX(),
    "Launch on login StartupTask enabled"
  );

  launchOnLoginCheckbox.click();
  ok(!launchOnLoginCheckbox.checked, "Autostart checkbox unchecked");

  ok(
    await !WindowsLaunchOnLogin.getLaunchOnLoginEnabledMSIX(),
    "Launch on login StartupTask disabled"
  );

  gBrowser.removeCurrentTab();
  await doCleanup();
});

add_task(async function enable_external_startuptask() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  // Ensure the task is disabled before enabling it
  await WindowsLaunchOnLogin.disableLaunchOnLoginMSIX();
  let enabled = await WindowsLaunchOnLogin.enableLaunchOnLoginMSIX();
  ok(enabled, "Task is enabled");

  // Open preferences to general pane
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
  ok(launchOnLoginCheckbox.checked, "Autostart checkbox automatically checked");

  gBrowser.removeCurrentTab();
  await doCleanup();
});

add_task(async function disable_external_startuptask() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  // Disable the startup task to ensure it's reflected in the settings
  await WindowsLaunchOnLogin.disableLaunchOnLoginMSIX();

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
  await doCleanup();
});
