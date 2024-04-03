/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps */
async function testSteps() {
  // A constant used to deal with small decrease in usage when transactions are
  // transferred from the WAL file back into the original database, also called
  // as checkpointing.
  const cosmologicalConstant = 35000;

  // The maximum number of threads that can be used for database activity at a
  // single time.
  const maxConnectionThreadCount = 20;

  // The length of time that database connections will be held open after all
  // transactions have completed before doing idle maintenance.
  const connectionIdleMaintenanceMS = 2 * 1000;

  const name = "test_connection_idle_maintenance_stop";
  const abc = "abcdefghijklmnopqrstuvwxyz";

  // IndexedDB on Android does `PRAGMA auto_vacuum = FULL`, so the freelist
  // pages are moved to the end of the database file and the database file is
  // truncated to remove the freelist pages at every transaction commit.
  if (mozinfo.os == "android") {
    info("Test disabled on Android for now");
    return;
  }

  info("Setting pref");

  Services.prefs.setIntPref(
    "dom.indexedDB.connectionIdleMaintenance.pauseOnConnectionThreadMs",
    2 * connectionIdleMaintenanceMS
  );

  info("Forcing only one connection thread to be available");

  let done = false;

  // Create databases which will continuously keep their connection threads
  // busy.
  const completePromises = await (async function () {
    let promises = [];

    for (let index = 0; index < maxConnectionThreadCount - 1; index++) {
      const request = indexedDB.open(name + "-" + index, 1);

      {
        const event = await expectingUpgrade(request);

        const database = event.target.result;

        const objectStore = database.createObjectStore(name);

        objectStore.add("foo", 42);
      }

      const event = await expectingSuccess(request);

      const database = event.target.result;

      const transaction = database.transaction(name);

      const objectStore = transaction.objectStore(name);

      function doWork() {
        const request = objectStore.get(42);
        request.onsuccess = function () {
          if (!done) {
            doWork();
          }
        };
      }

      doWork();

      const promise = new Promise(function (resolve) {
        transaction.oncomplete = resolve;
      });

      promises.push(promise);
    }

    return promises;
  })();

  info("Creating database A");

  // Create a database which will be used for stopping of the connection idle
  // maintenance.
  const databaseA = await (async function () {
    const request = indexedDB.open(name + "-a", 1);

    {
      const event = await expectingUpgrade(request);

      const database = event.target.result;

      database.createObjectStore(name);
    }

    const event = await expectingSuccess(request);

    const database = event.target.result;

    return database;
  })();

  info("Creating database B");

  // Create a database for checking of the connection idle maintenance.
  {
    const request = indexedDB.open(name + "-b", 1);

    const event = await expectingUpgrade(request);

    const database = event.target.result;

    const objectStore = database.createObjectStore(name);

    // Add lots of data...
    for (let index = 0; index < 10000; index++) {
      objectStore.add(abc, index);
    }

    // And then clear it so that maintenance has some space to reclaim.
    objectStore.clear();

    await expectingSuccess(request);
  }

  info("Getting database usage before maintenance");

  const databaseUsageBeforeMaintenance = await new Promise(function (resolve) {
    getCurrentUsage(function (request) {
      resolve(request.result.databaseUsage);
    });
  });

  info("Waiting for maintenance to start");

  // This time is a double of connectionIdleMaintenanceMS which should be
  // pessimistic enough to work with randomly slowed down threads in the
  // chaos mode.
  await new Promise(function (resolve) {
    do_timeout(2 * connectionIdleMaintenanceMS, resolve);
  });

  info("Activating database A");

  // Activate an open database which should trigger stopping of the connection
  // idle maintenance of the database which had a lot of data.
  {
    const transaction = databaseA.transaction(name);

    const objectStore = transaction.objectStore(name);

    const request = objectStore.get(42);

    await requestSucceeded(request);
  }

  info("Waiting for maintenance to finish");

  // This time is a double of connectionIdleMaintenanceMS which should be
  // pessimistic enough to work with randomly slowed down threads in the
  // chaos mode.
  await new Promise(function (resolve) {
    do_timeout(2 * connectionIdleMaintenanceMS, resolve);
  });

  info("Getting database usage after maintenance");

  const databaseUsageAfterMaintenance = await new Promise(function (resolve) {
    getCurrentUsage(function (request) {
      resolve(request.result.databaseUsage);
    });
  });

  info(
    "Database usage before: " +
      databaseUsageBeforeMaintenance +
      ". " +
      "Database usage after: " +
      databaseUsageAfterMaintenance
  );

  // Checkpointing done immediately after the maintenance can slightly decrease
  // the usage even when the maintenance was stopped very early.
  ok(
    databaseUsageBeforeMaintenance - databaseUsageAfterMaintenance <
      cosmologicalConstant,
    "Maintenance did not significantly decrease database usage"
  );

  done = true;

  info("Waiting for transactions to complete");

  await Promise.all(completePromises);
}
