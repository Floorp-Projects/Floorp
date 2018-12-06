/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { adbAddon } = require("devtools/shared/adb/adb-addon");
const { adbProcess } = require("devtools/shared/adb/adb-process");
const { check } = require("devtools/shared/adb/adb-running-checker");

/**
 * Check that ADB is stopped:
 * - when the adb extension is uninstalled
 * - when no consumer is registered
 */
add_task(async function() {
  await pushPref("devtools.remote.adb.extensionURL",
                 CHROME_URL_ROOT + "resources/test-adb-extension/adb-extension-#OS#.xpi");

  info("Check if ADB is already running before the test starts");
  const isAdbAlreadyRunning = await check();
  if (isAdbAlreadyRunning) {
    ok(false, "The ADB process is already running on this machine, it should be " +
      "stopped before running this test");
    return;
  }

  const { tab } = await openAboutDebugging();

  info("Install the adb extension and wait for ADB to start");
  // Use "internal" as the install source to avoid triggering telemetry.
  adbAddon.install("internal");
  await waitForAdbStart();

  info("Open a second about:debugging");
  const { tab: secondTab } = await openAboutDebugging();

  info("Close the second about:debugging and check that ADB is still running");
  await removeTab(secondTab);
  ok(await check(), "ADB is still running");

  await removeTab(tab);

  info("Check that the adb process stops after closing about:debugging");
  await waitForAdbStop();

  info("Open a third about:debugging, wait for the ADB to start again");
  const { tab: thirdTab } = await openAboutDebugging();
  await waitForAdbStart();

  info("Uninstall the addon, this should stop ADB as well");
  adbAddon.uninstall();
  await waitForAdbStop();

  info("Reinstall the addon, this should start ADB again");
  adbAddon.install("internal");
  await waitForAdbStart();

  info("Close the last tab, this should stop ADB");
  await removeTab(thirdTab);
  await waitForAdbStop();
});

async function waitForAdbStart() {
  info("Wait for ADB to start");
  return asyncWaitUntil(async () => {
    const isProcessReady = adbProcess.ready;
    const isRunning = await check();
    return isProcessReady && isRunning;
  });
}

async function waitForAdbStop() {
  info("Wait for ADB to stop");
  return asyncWaitUntil(async () => {
    const isProcessReady = adbProcess.ready;
    const isRunning = await check();
    return !isProcessReady && !isRunning;
  });
}
