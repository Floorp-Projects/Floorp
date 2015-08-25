/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ?
               window.location.pathname :
               "test_temporary_storage.js";
  const finalVersion = 2;

  const tempStorageLimitKB = 1024;
  const checkpointSleepTimeSec = 5;

  function setLimit(limit) {
    const pref = "dom.quotaManager.temporaryStorage.fixedLimit";
    if (limit) {
      info("Setting temporary storage limit to " + limit);
      SpecialPowers.setIntPref(pref, limit);
    } else {
      info("Removing temporary storage limit");
      SpecialPowers.clearUserPref(pref);
    }
  }

  function getSpec(index) {
    return "http://foo" + index + ".com";
  }

  function getPrincipal(url) {
    let uri = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService)
                .newURI(url, null, null);
    return Cc["@mozilla.org/scriptsecuritymanager;1"]
             .getService(Ci.nsIScriptSecurityManager)
             .getNoAppCodebasePrincipal(uri);
  }

  for (let temporary of [true, false]) {
    info("Testing '" + (temporary ? "temporary" : "default") + "' storage");

    setLimit(tempStorageLimitKB);

    clearAllDatabases(continueToNextStepSync);
    yield undefined;

    info("Stage 1 - Creating empty databases until we reach the quota limit");

    let databases = [];
    let options = { version: finalVersion };
    if (temporary) {
      options.storage = "temporary";
    }

    while (true) {
      let spec = getSpec(databases.length);

      info("Opening database for " + spec + " with version " + options.version);

      let gotUpgradeNeeded = false;

      let request =
        indexedDB.openForPrincipal(getPrincipal(spec), name, options);
      request.onerror = function(event) {
        is(request.error.name, "QuotaExceededError", "Reached quota limit");
        event.preventDefault();
        testGenerator.send(false);
      }
      request.onupgradeneeded = function(event) {
        gotUpgradeNeeded = true;
      }
      request.onsuccess = function(event) {
        let db = event.target.result;
        is(db.version, finalVersion, "Correct version " + finalVersion);
        databases.push(db);
        testGenerator.send(true);
      }

      let shouldContinue = yield undefined;
      if (shouldContinue) {
        is(gotUpgradeNeeded, true, "Got upgradeneeded event");
        ok(true, "Got success event");
      } else {
        break;
      }
    }

    while (true) {
      info("Sleeping for " + checkpointSleepTimeSec + " seconds to let all " +
           "checkpoints finish so that we know we have reached quota limit");
      setTimeout(continueToNextStepSync, checkpointSleepTimeSec * 1000);
      yield undefined;

      let spec = getSpec(databases.length);

      info("Opening database for " + spec + " with version " + options.version);

      let gotUpgradeNeeded = false;

      let request =
        indexedDB.openForPrincipal(getPrincipal(spec), name, options);
      request.onerror = function(event) {
        is(request.error.name, "QuotaExceededError", "Reached quota limit");
        event.preventDefault();
        testGenerator.send(false);
      }
      request.onupgradeneeded = function(event) {
        gotUpgradeNeeded = true;
      }
      request.onsuccess = function(event) {
        let db = event.target.result;
        is(db.version, finalVersion, "Correct version " + finalVersion);
        databases.push(db);
        testGenerator.send(true);
      }

      let shouldContinue = yield undefined;
      if (shouldContinue) {
        is(gotUpgradeNeeded, true, "Got upgradeneeded event");
        ok(true, "Got success event");
      } else {
        break;
      }
    }

    let databaseCount = databases.length;
    info("Created " + databaseCount + " databases before quota limit reached");

    info("Stage 2 - " +
         "Closing all databases and then attempting to create one more, then " +
         "verifying that the oldest origin was cleared");

    for (let i = 0; i < databases.length; i++) {
      info("Closing database for " + getSpec(i));
      databases[i].close();

      // Timer resolution on Windows is low so wait for 40ms just to be safe.
      setTimeout(continueToNextStepSync, 40);
      yield undefined;
    }
    databases = null;

    let spec = getSpec(databaseCount);
    info("Opening database for " + spec + " with version " + options.version);

    let request = indexedDB.openForPrincipal(getPrincipal(spec), name, options);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;
    let event = yield undefined;

    is(event.type, "upgradeneeded", "Got upgradeneeded event");

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Got success event");

    let db = event.target.result;
    is(db.version, finalVersion, "Correct version " + finalVersion);
    db.close();
    db = null;

    setLimit(tempStorageLimitKB * 2);

    resetAllDatabases(continueToNextStepSync);
    yield undefined;

    delete options.version;

    spec = getSpec(0);
    info("Opening database for " + spec + " with unspecified version");

    request = indexedDB.openForPrincipal(getPrincipal(spec), name, options);
    request.onerror = errorHandler;
    request.onupgradeneeded = grabEventAndContinueHandler;
    request.onsuccess = unexpectedSuccessHandler;
    event = yield undefined;

    is(event.type, "upgradeneeded", "Got upgradeneeded event");

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Got success event");

    db = event.target.result;
    is(db.version, 1, "Correct version 1 (database was recreated)");
    db.close();
    db = null;

    info("Stage 3 - " +
         "Cutting storage limit in half to force deletion of some databases");

    setLimit(tempStorageLimitKB / 2);

    resetAllDatabases(continueToNextStepSync);
    yield undefined;

    info("Opening database for " + spec + " with unspecified version");

    // Open the same db again to force QM to delete others. The first origin (0)
    // should be the most recent so it should not be deleted and we should not
    // get an upgradeneeded event here.
    request = indexedDB.openForPrincipal(getPrincipal(spec), name, options);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    event = yield undefined;

    is(event.type, "success", "Got correct event type");

    db = event.target.result;
    is(db.version, 1, "Correct version 1");
    db.close();
    db = null;

    setLimit(tempStorageLimitKB * 2);

    resetAllDatabases(continueToNextStepSync);
    yield undefined;

    options.version = finalVersion;

    let newDatabaseCount = 0;
    for (let i = 0; i < databaseCount; i++) {
      let spec = getSpec(i);
      info("Opening database for " + spec + " with version " + options.version);

      let request =
        indexedDB.openForPrincipal(getPrincipal(spec), name, options);
      request.onerror = errorHandler;
      request.onupgradeneeded = function(event) {
        if (!event.oldVersion) {
          newDatabaseCount++;
        }
      }
      request.onsuccess = grabEventAndContinueHandler;
      let event = yield undefined;

      is(event.type, "success", "Got correct event type");

      let db = request.result;
      is(db.version, finalVersion, "Correct version " + finalVersion);
      db.close();
    }

    info("Needed to recreate " + newDatabaseCount + " databases");
    ok(newDatabaseCount, "Created some new databases");
    ok(newDatabaseCount < databaseCount, "Didn't recreate all databases");
  }

  finishTest();
  yield undefined;
}
