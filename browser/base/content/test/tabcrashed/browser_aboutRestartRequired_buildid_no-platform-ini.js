"use strict";

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(2);

SimpleTest.expectChildProcessCrash();

add_task(async function test_browser_crashed_no_platform_ini_event() {
  info("Waiting for oop-browser-buildid-mismatch event.");

  Services.telemetry.clearScalars();
  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  ok(await ensureBuildID(), "System has correct platform.ini");

  let profD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let platformIniOrig = await IOUtils.readUTF8(
    PathUtils.join(profD.path, "platform.ini")
  );

  await IOUtils.remove(PathUtils.join(profD.path, "platform.ini"));

  setBuildidMatchDontSendEnv();
  await forceCleanProcesses();
  let eventPromise = getEventPromise(
    "oop-browser-buildid-mismatch",
    "no-platform-ini"
  );
  let tab = await openNewTab(false);
  await eventPromise;
  await IOUtils.writeUTF8(
    PathUtils.join(profD.path, "platform.ini"),
    platformIniOrig,
    { flush: true }
  );
  unsetBuildidMatchDontSendEnv();

  is(
    getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );
  await closeTab(tab);
});
