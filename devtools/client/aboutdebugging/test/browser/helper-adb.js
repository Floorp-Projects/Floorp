/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function checkAdbNotRunning() {
  info("Check if ADB is already running before the test starts");
  const {
    check,
  } = require("devtools/client/shared/remote-debugging/adb/adb-running-checker");
  const isAdbAlreadyRunning = await check();
  if (isAdbAlreadyRunning) {
    throw new Error(
      "The ADB process is already running on this machine, it should be " +
        "stopped before running this test"
    );
  }
}
/* exported checkAdbNotRunning */

// Returns a promise that resolves when the adb process exists and is running.
async function waitForAdbStart() {
  info("Wait for ADB to start");
  const {
    adbProcess,
  } = require("devtools/client/shared/remote-debugging/adb/adb-process");
  const {
    check,
  } = require("devtools/client/shared/remote-debugging/adb/adb-running-checker");
  return asyncWaitUntil(async () => {
    const isProcessReady = adbProcess.ready;
    const isRunning = await check();
    return isProcessReady && isRunning;
  });
}
/* exported waitForAdbStart */

// Attempt to stop ADB. Will only work if ADB was started by the current Firefox instance.
// Returns a promise that resolves when the adb process is no longer running.
async function stopAdbProcess() {
  info("Attempt to stop ADB");
  const {
    adbProcess,
  } = require("devtools/client/shared/remote-debugging/adb/adb-process");
  await adbProcess.stop();

  info("Wait for ADB to stop");
  const {
    check,
  } = require("devtools/client/shared/remote-debugging/adb/adb-running-checker");
  return asyncWaitUntil(async () => {
    const isProcessReady = adbProcess.ready;
    const isRunning = await check();
    return !isProcessReady && !isRunning;
  });
}
/* exported stopAdbProcess */
