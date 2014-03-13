/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

/**
 * Name space for MobileMessageDB.jsm.  Only initialized after first call to
 * newMobileMessageDB.
 */
let MMDB;

/**
 * Create a new MobileMessageDB instance.
 *
 * @return A MobileMessageDB instance.
 */
function newMobileMessageDB() {
  if (!MMDB) {
    MMDB = Cu.import("resource://gre/modules/MobileMessageDB.jsm", {});
    is(typeof MMDB.MobileMessageDB, "function", "MMDB.MobileMessageDB");
  }

  let mmdb = new MMDB.MobileMessageDB();
  ok(mmdb, "MobileMessageDB instance");
  return mmdb;
}

/**
 * Initialize a MobileMessageDB.  Resolve if initialized with success, reject
 * otherwise.
 *
 * Fulfill params: a MobileMessageDB instance.
 * Reject params: a MobileMessageDB instance.
 *
 * @param aMmdb
 *        A MobileMessageDB instance.
 * @param aDbName
 *        A string name for that database.
 * @param aDbVersion
 *        The version that MobileMessageDB should upgrade to. 0 for the lastest
 *        version.
 *
 * @return A deferred promise.
 */
function initMobileMessageDB(aMmdb, aDbName, aDbVersion) {
  let deferred = Promise.defer();

  aMmdb.init(aDbName, aDbVersion, function(aError) {
    if (aError) {
      deferred.reject(aMmdb);
    } else {
      deferred.resolve(aMmdb);
    }
  });

  return deferred.promise;
}

/**
 * Close a MobileMessageDB.
 *
 * @param aMmdb
 *        A MobileMessageDB instance.
 *
 * @return The passed MobileMessageDB instance.
 */
function closeMobileMessageDB(aMmdb) {
  aMmdb.close();
  return aMmdb;
}

// A reference to a nsIUUIDGenerator service.
let _uuidGenerator;

/**
 * Generate a new UUID.
 *
 * @return A UUID string.
 */
function newUUID() {
  if (!_uuidGenerator) {
    _uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                     .getService(Ci.nsIUUIDGenerator);
    ok(_uuidGenerator, "uuidGenerator");
  }

  return _uuidGenerator.generateUUID().toString();
}

/**
 * Flush permission settings and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, "permissions flushed");

  finish();
}

/**
 * Basic test routine helper for mobile message tests.
 *
 * This helper does nothing but clean-ups.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  Promise.resolve()
    .then(aTestCaseMain)
    .then(null, function() {
      ok(false, 'promise rejects during test.');
    })
    .then(cleanUp);
}
