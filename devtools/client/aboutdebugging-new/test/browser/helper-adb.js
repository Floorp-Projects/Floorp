/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function checkAdbNotRunning() {
  info("Check if ADB is already running before the test starts");
  const { check } = require("devtools/shared/adb/adb-running-checker");
  const isAdbAlreadyRunning = await check();
  if (isAdbAlreadyRunning) {
    throw new Error("The ADB process is already running on this machine, it should be " +
      "stopped before running this test");
  }
}
/* exported checkAdbNotRunning */

// Returns a promise that resolves when the adb process exists and is running.
async function waitForAdbStart() {
  info("Wait for ADB to start");
  const { adbProcess } = require("devtools/shared/adb/adb-process");
  const { check } = require("devtools/shared/adb/adb-running-checker");
  return asyncWaitUntil(async () => {
    const isProcessReady = adbProcess.ready;
    const isRunning = await check();
    return isProcessReady && isRunning;
  });
}
/* exported waitForAdbStart */

// Returns a promise that resolves when the adb process is no longer running.
async function waitForAdbStop() {
  info("Wait for ADB to stop");
  const { adbProcess } = require("devtools/shared/adb/adb-process");
  const { check } = require("devtools/shared/adb/adb-running-checker");
  return asyncWaitUntil(async () => {
    const isProcessReady = adbProcess.ready;
    const isRunning = await check();
    return !isProcessReady && !isRunning;
  });
}
/* exported waitForAdbStop */
