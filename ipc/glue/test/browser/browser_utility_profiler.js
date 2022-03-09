/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from /tools/profiler/tests/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/tools/profiler/tests/browser/shared-head.js",
  this
);

startUtilityProcess();

add_task(async () => {
  info("Start the profiler");
  startProfiler();

  let profile;
  await TestUtils.waitForCondition(async () => {
    profile = await Services.profiler.getProfileDataAsync();
    return (
      profile.processes.filter(ps => ps.threads[0].processType === "utility")
        .length === 1
    );
  }, "Give time for the profiler to start and collect some samples");

  info(`Check that the utility process ${utilityPid} is present.`);
  let utilityProcessIndex = profile.processes.findIndex(
    p => p.threads[0].pid == utilityPid
  );
  Assert.notEqual(utilityProcessIndex, -1, "Could find index of utility");

  Assert.equal(
    profile.processes[utilityProcessIndex].threads[0].processType,
    "utility",
    "Profile has processType utility"
  );

  Assert.equal(
    profile.processes[utilityProcessIndex].threads[0].name,
    "GeckoMain",
    "Profile has correct main thread name"
  );

  Assert.equal(
    profile.processes[utilityProcessIndex].threads[0].processName,
    "Utility Process",
    "Profile has correct process name"
  );

  Assert.greater(
    profile.processes[utilityProcessIndex].threads.length,
    0,
    "The utility process should have threads"
  );

  Assert.equal(
    profile.threads.length,
    1,
    "The parent process should have only one thread"
  );

  Services.profiler.StopProfiler();
});

cleanUtilityProcessShutdown();
