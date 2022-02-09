/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var utilityPid = undefined;
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
  info("Hard kill Utility Process");
  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );
  ProcessTools.kill(utilityPid);
});
