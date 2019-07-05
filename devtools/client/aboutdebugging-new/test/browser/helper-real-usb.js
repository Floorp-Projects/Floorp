/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-adb.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-adb.js", this);

async function getExpectedRuntime() {
  const runtimes = await getExpectedRuntimeAll();
  return runtimes[0];
}
/* exported getExpectedRuntime */

async function getExpectedRuntimeAll() {
  const runtimesPath = _getExpectedRuntimesPath();
  const currentPath = env.get("PWD");
  const path = `${currentPath}/${runtimesPath}`;
  info(`Load ${path}`);
  const buffer = await OS.File.read(path);
  const data = new TextDecoder().decode(buffer);
  return JSON.parse(data);
}
/* exported getExpectedRuntimeAll */

function isAvailable() {
  return !!_getExpectedRuntimesPath();
}
/* exported isAvailable */

async function openAboutDebuggingWithADB() {
  const { document, tab, window } = await openAboutDebugging();

  await pushPref(
    "devtools.remote.adb.extensionURL",
    CHROME_URL_ROOT + "resources/test-adb-extension/adb-extension-#OS#.xpi"
  );
  await checkAdbNotRunning();

  const { adbAddon } = require("devtools/shared/adb/adb-addon");
  adbAddon.install("internal");
  const usbStatusElement = document.querySelector(".qa-sidebar-usb-status");
  await waitUntil(() => usbStatusElement.textContent.includes("USB enabled"));
  await waitForAdbStart();

  return { document, tab, window };
}
/* exported openAboutDebuggingWithADB */

function _getExpectedRuntimesPath() {
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  return env.get("USB_RUNTIMES");
}
