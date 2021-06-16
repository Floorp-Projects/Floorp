"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const kTimeout = 60 * 1000;

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(5);

async function waitForObserver(topic) {
  return new Promise(function(resolve, reject) {
    info("Setting up observer for '" + topic + "'");
    let interval = undefined;

    let obs = {
      observe() {
        info("Observed '" + topic + "'");
        clearInterval(interval);
        Services.obs.removeObserver(obs, topic);
        resolve();
      },
    };

    Services.obs.addObserver(obs, topic);

    let times = 0;
    let intervalMs = 1000;
    interval = setInterval(async () => {
      info("Opening and closing tab '" + topic + "'");
      await openNewTab(false);
      times += intervalMs;
      if (times > kTimeout) {
        clearInterval(interval);
        reject();
      }
    }, intervalMs);
  });
}

async function forceCleanProcesses() {
  const origPrefValue = SpecialPowers.getBoolPref(
    "dom.ipc.processPrelaunch.enabled"
  );
  info(
    "forceCleanProcesses: get dom.ipc.processPrelaunch.enabled=" + origPrefValue
  );
  await SpecialPowers.setBoolPref("dom.ipc.processPrelaunch.enabled", false);
  info("forceCleanProcesses: set dom.ipc.processPrelaunch.enabled=false");
  await SpecialPowers.setBoolPref(
    "dom.ipc.processPrelaunch.enabled",
    origPrefValue
  );
  info(
    "forceCleanProcesses: set dom.ipc.processPrelaunch.enabled=" + origPrefValue
  );
  const currPrefValue = SpecialPowers.getBoolPref(
    "dom.ipc.processPrelaunch.enabled"
  );
  ok(currPrefValue === origPrefValue, "processPrelaunch properly re-enabled");
}

add_task(async function test_browser_crashed_false_positive_event() {
  info("Waiting for oop-browser-crashed event.");

  Services.telemetry.clearScalars();
  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  try {
    setBuildidMatchDontSendEnv();
    await forceCleanProcesses();
    let eventPromise = getEventPromise(
      "oop-browser-crashed",
      "false-positive",
      kTimeout
    );
    await openNewTab(false);
    await eventPromise;
  } finally {
    unsetBuildidMatchDontSendEnv();
  }

  is(
    getFalsePositiveTelemetry(),
    1,
    "Build ID mismatch false positive count should be 1"
  );
});

add_task(async function test_browser_restartrequired_event() {
  info("Waiting for oop-browser-buildid-mismatch event.");

  Services.telemetry.clearScalars();
  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  let profD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let platformIniOrig = await OS.File.read(
    OS.Path.join(profD.path, "platform.ini"),
    { encoding: "utf-8" }
  );
  let buildID = Services.appinfo.platformBuildID;
  let platformIniNew = platformIniOrig.replace(buildID, "1234");

  await OS.File.writeAtomic(
    OS.Path.join(profD.path, "platform.ini"),
    platformIniNew,
    { encoding: "utf-8" }
  );

  try {
    setBuildidMatchDontSendEnv();
    await forceCleanProcesses();
    let eventPromise = getEventPromise(
      "oop-browser-buildid-mismatch",
      "buildid-mismatch",
      kTimeout
    );
    await openNewTab(false);

    await eventPromise;
  } finally {
    await OS.File.writeAtomic(
      OS.Path.join(profD.path, "platform.ini"),
      platformIniOrig,
      { encoding: "utf-8" }
    );

    unsetBuildidMatchDontSendEnv();
  }

  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );
});

add_task(async function test_browser_crashed_no_platform_ini_event() {
  info("Waiting for oop-browser-buildid-mismatch event.");

  Services.telemetry.clearScalars();
  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  let profD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let platformIniOrig = await OS.File.read(
    OS.Path.join(profD.path, "platform.ini"),
    { encoding: "utf-8" }
  );

  await OS.File.remove(OS.Path.join(profD.path, "platform.ini"));

  try {
    setBuildidMatchDontSendEnv();
    await forceCleanProcesses();
    let eventPromise = getEventPromise(
      "oop-browser-buildid-mismatch",
      "no-platform-ini",
      kTimeout
    );
    await openNewTab(false);

    await eventPromise;
  } finally {
    await OS.File.writeAtomic(
      OS.Path.join(profD.path, "platform.ini"),
      platformIniOrig,
      { encoding: "utf-8" }
    );

    unsetBuildidMatchDontSendEnv();
  }

  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );
});
