/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  let profd = Services.env.get("XPCSHELL_TEST_PROFILE_DIR");
  Services.env.set("XPCSHELL_TEST_PROFILE_DIR", "0");

  let uri = Services.io.newURI("https://www.example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  info("Setting permissions");

  Services.perms.addFromPrincipal(
    principal,
    "indexedDB",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  // The idle-daily notification is disabled in xpchsell tests, so we don't
  // need to do anything special to disable it for this test.

  info("Activating real idle service");

  do_get_idle();

  info("Creating databases");

  // Keep at least one database open.
  let req = indexedDB.open("foo-a", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  // Keep at least one factory operation alive by deleting a database that is
  // stil open.
  req = indexedDB.open("foo-b", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  indexedDB.deleteDatabase("foo-b");

  // Create a database which we will later try to open while maintenance is
  // performed.
  req = indexedDB.open("foo-c", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let dbC = event.target.result;
  dbC.close();

  let dbCount = 0;

  for (let persistence of ["persistent", "temporary", "default"]) {
    for (let i = 1; i <= 5; i++) {
      let dbName = "foo-" + i;
      let dbPersistence = persistence;
      let req = indexedDB.openForPrincipal(principal, dbName, {
        version: 1,
        storage: dbPersistence,
      });
      req.onerror = event => {
        if (dbPersistence != "persistent") {
          errorHandler(event);
          return;
        }

        // Explicit persistence is currently blocked on mobile.
        info(
          "Failed to create persistent database '" +
            dbPersistence +
            "/" +
            dbName +
            "', hopefully this is on mobile!"
        );

        event.preventDefault();

        if (!--dbCount) {
          continueToNextStep();
        }
      };
      req.onupgradeneeded = event => {
        let db = event.target.result;
        let objectStore = db.createObjectStore("foo");

        // Add lots of data...
        for (let j = 0; j < 10000; j++) {
          objectStore.add("abcdefghijklmnopqrstuvwxyz0123456789", j);
        }

        // And then clear it so that maintenance has some space to reclaim.
        objectStore.clear();
      };
      req.onsuccess = event => {
        let db = event.target.result;
        ok(db, "Created database '" + dbPersistence + "/" + dbName + "'");

        db.close();

        if (!--dbCount) {
          continueToNextStep();
        }
      };
      dbCount++;
    }
  }

  info("Waiting for in-flight operations");
  setTimeout(continueToNextStep, 2000);
  yield undefined;

  info("Getting usage before maintenance");

  let usageBeforeMaintenance;

  Services.qms.getUsageForPrincipal(principal, request => {
    let usage = request.result.usage;
    ok(usage > 0, "Usage is non-zero");
    usageBeforeMaintenance = usage;
    continueToNextStep();
  });

  setTimeout(continueToNextStep, 2000);
  yield undefined;

  // The connection may not be interrupted every time this test is run because
  // is is possible that maintenance completes before a connection interrupt
  // is triggered. In these cases, the test succeeds when maintenance succeeds.
  //
  // If there is a regression related to the interruption stack,
  // this test will begin to emit intermittent failures.
  info("Delay maintenance with requests to keep it alive until interrupted");

  info("Sending fake 'idle-daily' notification to QuotaManager");

  let observer = Services.qms.QueryInterface(Ci.nsIObserver);
  observer.observe(null, "idle-daily", "");

  info("Opening a database while maintenance is performed");

  req = indexedDB.open("foo-c", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;

  info("Sending another fake 'idle-daily' notification to QuotaManager");
  observer.observe(null, "idle-daily", "");

  info("Opening database again while maintenance is performed");

  req = indexedDB.open("foo-c", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;

  setTimeout(continueToNextStep, 2000);
  yield undefined;

  info("Sending one more fake 'idle-daily' notification to QuotaManager");
  observer.observe(null, "idle-daily", "");

  // This time is totally arbitrary. Most likely directory scanning will have
  // completed, QuotaManager locks will be acquired, and  maintenance tasks will
  // be scheduled before this time has elapsed, so we will be testing the
  // maintenance code. However, if something is slow then this will test
  // shutting down in the middle of maintenance.

  info("Waiting for maintenance to start");
  setTimeout(continueToNextStep, 10000);
  yield undefined;

  info("Getting usage after maintenance");

  let usageAfterMaintenance;

  Services.qms.getUsageForPrincipal(principal, request => {
    let usage = request.result.usage;
    ok(usage > 0, "Usage is non-zero");
    usageAfterMaintenance = usage;
    info(
      "Usage before: " +
        usageBeforeMaintenance +
        ". " +
        "Usage after: " +
        usageAfterMaintenance
    );
    ok(
      usageAfterMaintenance <= usageBeforeMaintenance,
      "Maintenance decreased file sizes or left them the same"
    );
    continueToNextStep();
  });
  yield undefined;

  Services.env.set("XPCSHELL_TEST_PROFILE_DIR", profd);
  finishTest();
  yield undefined;
}
