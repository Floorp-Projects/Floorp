/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const utilityProcessTest = Cc[
  "@mozilla.org/utility-process-test;1"
].createInstance(Ci.nsIUtilityProcessTest);

add_task(async () => {
  await utilityProcessTest
    .startProcess()
    .then(async pid => {
      ok(true, "Could start Utility process: " + pid);
    })
    .catch(async () => {
      ok(false, "Cannot start Utility process?");
    });
});

add_task(async () => {
  info("CleanShutdown Utility Process");
  await utilityProcessTest.stopProcess();
});
