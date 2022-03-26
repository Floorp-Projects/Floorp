/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var utilityPid = undefined;
const utilityProcessTest = Cc[
  "@mozilla.org/utility-process-test;1"
].createInstance(Ci.nsIUtilityProcessTest);

const kGenericUtility = 0x0;

function startUtilityProcess() {
  add_task(async () => {
    info("Start a UtilityProcess");
    await utilityProcessTest
      .startProcess()
      .then(async pid => {
        utilityPid = pid;
        ok(true, "Could start Utility process: " + pid);
      })
      .catch(async () => {
        ok(false, "Cannot start Utility process?");
      });
  });
}

function cleanUtilityProcessShutdown() {
  add_task(async () => {
    const utilityProcessGone = TestUtils.topicObserved("ipc:utility-shutdown");

    info("CleanShutdown Utility Process");
    await utilityProcessTest.stopProcess();

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

    ok(!subject.hasKey("dumpID"), "There should be no dumpID");
  });
}
