/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// When running full suite, previous audio decoding tests might have left some
// running and this might interfere with our testing
add_setup(async function ensureNoExistingProcess() {
  await killUtilityProcesses();
});

add_task(async () => {
  const utilityPid = await startUtilityProcess();

  const gMgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager
  );
  Assert.notStrictEqual(
    utilityPid,
    undefined,
    `Utility process is running as ${utilityPid}`
  );

  var utilityReports = [];

  const performCollection = new Promise(resolve => {
    // Record the reports from the live memory reporters then process them.
    let handleReport = function (
      aProcess,
      aUnsafePath,
      aKind,
      aUnits,
      aAmount,
      aDescription
    ) {
      const expectedProcess = `Utility (pid ${utilityPid}, sandboxingKind ${kGenericUtilitySandbox})`;
      if (aProcess !== expectedProcess) {
        return;
      }

      let report = {
        process: aProcess,
        path: aUnsafePath,
        kind: aKind,
        units: aUnits,
        amount: aAmount,
        description: aDescription,
      };

      utilityReports.push(report);
    };

    info("Memory report: Perform the call");
    gMgr.getReports(handleReport, null, resolve, null, false);
  });

  await performCollection;

  info(
    `Collected ${utilityReports.length} reports from utility process ${utilityPid}`
  );
  ok(!!utilityReports.length, "Collected some reports");
  Assert.strictEqual(
    utilityReports.filter(r => r.path === "vsize" && r.amount > 0).length,
    1,
    "Collected vsize report"
  );
  Assert.strictEqual(
    utilityReports.filter(r => r.path === "resident" && r.amount > 0).length,
    1,
    "Collected resident report"
  );
  ok(
    !!utilityReports.filter(
      r => r.path.search(/^explicit\/.*/) >= 0 && r.amount > 0
    ).length,
    "Collected some explicit/ report"
  );

  await cleanUtilityProcessShutdown();
});
