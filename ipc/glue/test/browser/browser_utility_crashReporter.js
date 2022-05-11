/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  SimpleTest.expectChildProcessCrash();
  const utilityPid = await startUtilityProcess();

  const crashMan = Services.crashmanager;
  const utilityProcessGone = TestUtils.topicObserved("ipc:utility-shutdown");

  info("prune any previous crashes");
  const future = new Date(Date.now() + 1000 * 60 * 60 * 24);
  await crashMan.pruneOldCrashes(future);

  info("crash Utility Process");
  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );
  ProcessTools.crash(utilityPid);

  info("Waiting for utility process to go away.");
  let [subject, data] = await utilityProcessGone;
  ok(
    parseInt(data, 10) === utilityPid,
    `Should match the crashed PID ${utilityPid} with ${data}`
  );
  ok(
    subject instanceof Ci.nsIPropertyBag2,
    "Subject needs to be a nsIPropertyBag2 to clean up properly"
  );

  const dumpID = subject.getPropertyAsAString("dumpID");
  ok(dumpID, "There should be a dumpID");

  await crashMan.ensureCrashIsPresent(dumpID);
  await crashMan.getCrashes().then(crashes => {
    is(crashes.length, 1, "There should be only one record");
    const crash = crashes[0];
    ok(
      crash.isOfType(
        crashMan.processTypes[Ci.nsIXULRuntime.PROCESS_TYPE_UTILITY],
        crashMan.CRASH_TYPE_CRASH
      ),
      "Record should be a utility process crash"
    );
    ok(crash.id === dumpID, "Record should have an ID");
    ok(
      parseInt(crash.metadata.UtilityProcessSandboxingKind, 10) ===
        kGenericUtility,
      "Record should have the sandboxing kind value"
    );
  });

  let minidumpDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  minidumpDirectory.append("minidumps");

  let dumpfile = minidumpDirectory.clone();
  dumpfile.append(dumpID + ".dmp");
  if (dumpfile.exists()) {
    info(`Removal of ${dumpfile.path}`);
    dumpfile.remove(false);
  }

  let extrafile = minidumpDirectory.clone();
  extrafile.append(dumpID + ".extra");
  info(`Removal of ${extrafile.path}`);
  if (extrafile.exists()) {
    extrafile.remove(false);
  }
});
