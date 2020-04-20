/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Ensures that content crashes are reported to the crash service
// (nsICrashService and CrashManager.jsm).

/* eslint-disable mozilla/no-arbitrary-setTimeout */
SimpleTest.requestFlakyTimeout("untriaged");

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    forceNewProcess: true,
  });

  SimpleTest.expectChildProcessCrash();

  let crashMan = Services.crashmanager;

  // First, clear the crash record store.
  info("Waiting for pruneOldCrashes");
  var future = new Date(Date.now() + 1000 * 60 * 60 * 24);
  await crashMan.pruneOldCrashes(future);

  var crashDateMS = Date.now();

  let crashPromise = BrowserTestUtils.crashFrame(tab.linkedBrowser);

  // Finally, poll for the new crash record.
  await new Promise((resolve, reject) => {
    function tryGetCrash() {
      info("Waiting for getCrashes");
      crashMan.getCrashes().then(
        function(crashes) {
          if (crashes.length) {
            is(crashes.length, 1, "There should be only one record");
            var crash = crashes[0];
            ok(
              crash.isOfType(
                crashMan.PROCESS_TYPE_CONTENT,
                crashMan.CRASH_TYPE_CRASH
              ),
              "Record should be a content crash"
            );
            ok(!!crash.id, "Record should have an ID");
            ok(!!crash.crashDate, "Record should have a crash date");
            var dateMS = crash.crashDate.valueOf();
            var twoMin = 1000 * 60 * 2;
            ok(
              crashDateMS - twoMin <= dateMS && dateMS <= crashDateMS + twoMin,
              `Record's crash date should be nowish: ` +
                `now=${crashDateMS} recordDate=${dateMS}`
            );
            resolve();
          } else {
            setTimeout(tryGetCrash, 1000);
          }
        },
        function(err) {
          reject(err);
        }
      );
    }
    setTimeout(tryGetCrash, 1000);
  });

  await crashPromise;

  await BrowserTestUtils.removeTab(tab);
});
