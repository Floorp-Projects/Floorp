/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var utilityPid = undefined;
var utilityReports = [];

add_task(async () => {
  const utilityProcessTest = Cc[
    "@mozilla.org/utility-process-test;1"
  ].createInstance(Ci.nsIUtilityProcessTest);
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

add_task(async () => {
  const gMgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager
  );
  ok(utilityPid !== undefined, "Utility process is running");

  const performCollection = new Promise((resolve, reject) => {
    // Record the reports from the live memory reporters then process them.
    let handleReport = function(
      aProcess,
      aUnsafePath,
      aKind,
      aUnits,
      aAmount,
      aDescription
    ) {
      const expectedProcess = `Utility (pid ${utilityPid})`;
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

  info("Collected", utilityReports.length, "reports from utility process");
  ok(!!utilityReports.length, "Collected some reports");
  ok(
    utilityReports.filter(r => r.path === "vsize" && r.amount > 0).length === 1,
    "Collected vsize report"
  );
  ok(
    utilityReports.filter(r => r.path === "resident" && r.amount > 0).length ===
      1,
    "Collected resident report"
  );
  ok(
    !!utilityReports.filter(
      r => r.path.search(/^explicit\/.*/) >= 0 && r.amount > 0
    ).length,
    "Collected some explicit/ report"
  );
});
