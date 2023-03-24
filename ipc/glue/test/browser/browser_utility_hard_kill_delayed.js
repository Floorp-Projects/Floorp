/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  const utilityPid = await startUtilityProcess();

  SimpleTest.expectChildProcessCrash();

  const utilityProcessGone = TestUtils.topicObserved("ipc:utility-shutdown");

  info("Hard kill Utility Process");
  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );

  // Here we really want to exercise the fact that kill() might not be done
  // right now but a bit later, and we should wait for the process to be dead
  // before considering the test is finished.
  //
  // Without this, we get into bug 1754572 (where there was no setTimeout nor
  // the wait) where the kill() operation ends up really killing the child a
  // bit after the current test has been finished ; unfortunately, this happened
  // right after the next test, browser_utility_memoryReport.js did start and
  // even worse, after it thought it had started a new utility process. We were
  // in fact re-using the one we started here, and when we wanted to query its
  // pid in the browser_utility_memoryReport.js then the kill() happened, so
  // no more process and the test intermittently failed.
  //
  // The timeout value of 50ms should be long enough to allow the test to finish
  // and the next one to start and get a reference on the process we launched,
  // and yet allow us to kill the process in the middle of the next test. Higher
  // values would allow browser_utility_memoryReport.js to complete without
  // reproducing the issue (both locally and on try).
  //
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => {
    ProcessTools.kill(utilityPid);
  }, 50);

  info(`Waiting for utility process ${utilityPid} to go away.`);
  let [subject, data] = await utilityProcessGone;
  ok(
    subject instanceof Ci.nsIPropertyBag2,
    "Subject needs to be a nsIPropertyBag2 to clean up properly"
  );
  is(
    parseInt(data, 10),
    utilityPid,
    `Should match the crashed PID ${utilityPid} with ${data}`
  );

  // Make sure the process is dead, otherwise there is a risk of race for
  // writing leak logs
  utilityProcessTest().noteIntentionalCrash(utilityPid);
});
