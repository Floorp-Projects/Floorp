/* exported Task, startServerAndGetSelectedTabMemory, destroyServerAndFinish,
   waitForTime, waitUntil */
"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
var gReduceTimePrecision = Services.prefs.getBoolPref(
  "privacy.reduceTimerPrecision"
);
Services.prefs.setBoolPref("privacy.reduceTimerPrecision", false);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
  Services.prefs.setBoolPref(
    "privacy.reduceTimerPrecision",
    gReduceTimePrecision
  );
});

async function getTargetForSelectedTab() {
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const commands = await CommandsFactory.forTab(
    browserWindow.gBrowser.selectedTab
  );
  await commands.targetCommand.startListening();
  const isEveryFrameTargetEnabled = Services.prefs.getBoolPref(
    "devtools.every-frame-target.enabled",
    false
  );
  if (!isEveryFrameTargetEnabled) {
    return commands.targetCommand.targetFront;
  }

  // If EFT is enabled, we need to retrieve the target of the test document
  const targets = await commands.targetCommand.getAllTargets([
    commands.targetCommand.TYPES.FRAME,
  ]);

  return targets.find(t => t.url !== "chrome://mochikit/content/harness.xhtml");
}

async function startServerAndGetSelectedTabMemory() {
  const target = await getTargetForSelectedTab();
  const memory = await target.getFront("memory");
  return { memory, target };
}

async function destroyServerAndFinish(target) {
  await target.destroy();
  SimpleTest.finish();
}

function waitForTime(ms) {
  return new Promise((resolve, reject) => {
    setTimeout(resolve, ms);
  });
}

function waitUntil(predicate) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve =>
    setTimeout(() => waitUntil(predicate).then(() => resolve(true)), 10)
  );
}
