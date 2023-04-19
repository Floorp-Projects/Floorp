/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from /tools/profiler/tests/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/tools/profiler/tests/browser/shared-head.js",
  this
);

// When running full suite, previous tests may have left some utility
// processes running and this might interfere with our testing.
add_setup(async function ensureNoExistingProcess() {
  await killUtilityProcesses();
});

add_task(async () => {
  const utilityPid = await startUtilityProcess();

  info("Start the profiler");
  await startProfiler();

  let profile;
  await TestUtils.waitForCondition(async () => {
    profile = await Services.profiler.getProfileDataAsync();
    return (
      // Search for process name to not be disturbed by other types of utility
      // e.g. Utility AudioDecoder
      profile.processes.filter(
        ps => ps.threads[0].processName === "Utility Process"
      ).length === 1
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

  await cleanUtilityProcessShutdown();
});
