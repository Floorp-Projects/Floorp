/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const utilityProcessTest = Cc[
  "@mozilla.org/utility-process-test;1"
].createInstance(Ci.nsIUtilityProcessTest);

const kGenericUtility = 0x0;

async function startUtilityProcess() {
  info("Start a UtilityProcess");
  return utilityProcessTest.startProcess();
}

async function cleanUtilityProcessShutdown(utilityPid) {
  info(`CleanShutdown Utility Process ${utilityPid}`);
  ok(utilityPid !== undefined, "Utility needs to be defined");

  const utilityProcessGone = TestUtils.topicObserved(
    "ipc:utility-shutdown",
    (subject, data) => parseInt(data, 10) === utilityPid
  );
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
}
