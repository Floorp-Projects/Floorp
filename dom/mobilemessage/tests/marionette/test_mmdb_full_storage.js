/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_full_storage:" + newUUID();

let gIsDiskFull = true;

let gMessageToSave = {
  type: "sms",
  sender: "+0987654321",
  receiver: "+1234567890",
  body: "quick fox jump over the lazy dog",
  deliveryStatusRequested: false,
  timestamp: Date.now(),
  iccId: "1029384756"
};

function testSaveSendingMessage(aMmdb) {
  log("testSaveSendingMessage()");

  let deferred = Promise.defer();

  aMmdb.saveSendingMessage(gMessageToSave,
                          { notify : function(aRv, aDomMessage) {
    if (aRv === Cr.NS_ERROR_FILE_NO_DEVICE_SPACE) {
      ok(true, "Forbidden due to storage full.");
      deferred.resolve(aMmdb);
    } else {
      ok(false, "Unexpected result: " + aRv);
      deferred.reject(aRv);
    }
  }});

  return deferred.promise;
}

function testSaveReceivingMessage(aMmdb) {
  log("testSaveReceivingMessage()");

  let deferred = Promise.defer();

  aMmdb.saveReceivedMessage(gMessageToSave,
                            { notify : function(aRv, aDomMessage) {
    if (aRv === Cr.NS_ERROR_FILE_NO_DEVICE_SPACE) {
      ok(true, "Forbidden due to storage full.");
      deferred.resolve(aMmdb);
    } else {
      ok(false, "Unexpected result: " + aRv);
      deferred.reject(aRv);
    }
  }});

  return deferred.promise;
}

function testGetMessage(aMmdb) {
  log("testGetMessage()");

  let deferred = Promise.defer();

  aMmdb.getMessage(1,
                   { notifyGetMessageFailed : function(aRv) {
    if (aRv === Ci.nsIMobileMessageCallback.NOT_FOUND_ERROR) {
      ok(true, "Getting message successfully!");
      deferred.resolve(aMmdb);
    } else {
      ok(false, "Unexpected result: " + aRv);
      deferred.reject(aRv);
    }
  }});

  return deferred.promise;
}

startTestBase(function testCaseMain() {

  let mmdb = newMobileMessageDB();
  mmdb.isDiskFull = function() {
    return gIsDiskFull;
  };
  return initMobileMessageDB(mmdb, DBNAME, 0)
         .then(testSaveSendingMessage)
         .then(testSaveReceivingMessage)
         .then(testGetMessage)
         .then(closeMobileMessageDB);
});
