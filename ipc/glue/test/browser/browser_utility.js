/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  const utilityProcessTest = Cc[
    "@mozilla.org/utility-process-test;1"
  ].createInstance(Ci.nsIUtilityProcessTest);
  await utilityProcessTest
    .startProcess()
    .then(async () => {
      ok(true, "Could start Utility process!");
    })
    .catch(async () => {
      ok(false, "Cannot start Utility process?");
    });
});
