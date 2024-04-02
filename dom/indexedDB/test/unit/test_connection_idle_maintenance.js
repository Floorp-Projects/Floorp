/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps */
async function testSteps() {
  // A constant used to deal with small decrease in usage when transactions are
  // transferred from the WAL file back into the original database, also called
  // as checkpointing.
  const cosmologicalConstant = 20000;

  // The length of time that database connections will be held open after all
  // transactions have completed before doing idle maintenance.
  const connectionIdleMaintenanceMS = 2 * 1000;

  const name = "test_connection_idle_maintenance";
  const abc = "abcdefghijklmnopqrstuvwxyz";

  // IndexedDB on Android does `PRAGMA auto_vacuum = FULL`, so the freelist
  // pages are moved to the end of the database file and the database file is
  // truncated to remove the freelist pages at every transaction commit.
  if (mozinfo.os == "android") {
    info("Test disabled on Android for now");
    return;
  }

  info("Creating database");

  let request = indexedDB.open(name, 1);

  let event = await expectingUpgrade(request);

  let database = event.target.result;

  let objectStore = database.createObjectStore(name);

  // Add lots of data...
  for (let i = 0; i < 10000; i++) {
    objectStore.add(abc, i);
  }

  // And then clear it so that maintenance has some space to reclaim.
  objectStore.clear();

  await expectingSuccess(request);

  info("Getting database usage before maintenance");

  let databaseUsageBeforeMaintenance = await new Promise(function (resolve) {
    getCurrentUsage(function (request) {
      resolve(request.result.databaseUsage);
    });
  });

  info("Waiting for maintenance to start");

  // This time is a double of connectionIdleMaintenanceMS which should be
  // pessimistic enough to work with randomly slowed down threads in the chaos
  // mode.
  await new Promise(function (resolve) {
    do_timeout(2 * connectionIdleMaintenanceMS, resolve);
  });

  info("Waiting for maintenance to finish");

  // This time is a double of connectionIdleMaintenanceMS which should be
  // pessimistic enough to work with randomly slowed down threads in the chaos
  // mode.
  await new Promise(function (resolve) {
    do_timeout(2 * connectionIdleMaintenanceMS, resolve);
  });

  info("Getting database usage after maintenance");

  let databaseUsageAfterMaintenance = await new Promise(function (resolve) {
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
  // the usage even when the maintenance was not run at all.
  ok(
    databaseUsageBeforeMaintenance - databaseUsageAfterMaintenance >=
      cosmologicalConstant,
    "Maintenance significantly decreased database usage"
  );
}
