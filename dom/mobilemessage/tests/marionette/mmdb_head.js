/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";

var Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

/**
 * Name space for MobileMessageDB.jsm.  Only initialized after first call to
 * newMobileMessageDB.
 */
var MMDB;

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

/**
 * Utility function for calling MMDB methods that takes either a
 * nsIGonkMobileMessageDatabaseCallback or a
 * nsIGonkMobileMessageDatabaseRecordCallback.
 *
 * Resolve when the target method notifies us with a successful result code;
 * reject otherwise. In either case, the arguments passed are packed into an
 * array and propagated to next action.
 *
 * Fulfill params: an array whose elements are the arguments of the original
 *                 callback.
 * Reject params: same as fulfill params.
 *
 * @param aMmdb
 *        A MobileMessageDB instance.
 * @param aMethodName
 *        A string name for that target method.
 * @param ...
 *        Extra arguments to pass to that target method. The last callback
 *        argument should always be excluded.
 *
 * @return A deferred promise.
 */
function callMmdbMethod(aMmdb, aMethodName) {
  let deferred = Promise.defer();

  let args = Array.slice(arguments, 2);
  args.push({
    notify: function(aRv) {
      if (!Components.isSuccessCode(aRv)) {
        ok(true, aMethodName + " returns a unsuccessful code: " + aRv);
        deferred.reject(Array.slice(arguments));
      } else {
        ok(true, aMethodName + " returns a successful code: " + aRv);
        deferred.resolve(Array.slice(arguments));
      }
    }
  });
  aMmdb[aMethodName].apply(aMmdb, args);

  return deferred.promise;
}

/**
 * A convenient function for calling |mmdb.saveSendingMessage(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function saveSendingMessage(aMmdb, aMessage) {
  return callMmdbMethod(aMmdb, "saveSendingMessage", aMessage);
}

/**
 * A convenient function for calling |mmdb.saveReceivedMessage(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function saveReceivedMessage(aMmdb, aMessage) {
  return callMmdbMethod(aMmdb, "saveReceivedMessage", aMessage);
}

/**
 * A convenient function for calling |mmdb.setMessageDeliveryByMessageId(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function setMessageDeliveryByMessageId(aMmdb, aMessageId, aReceiver, aDelivery,
                                       aDeliveryStatus, aEnvelopeId) {
  return callMmdbMethod(aMmdb, "setMessageDeliveryByMessageId", aMessageId,
                        aReceiver, aDelivery, aDeliveryStatus, aEnvelopeId);
}

/**
 * A convenient function for calling
 * |mmdb.setMessageDeliveryStatusByEnvelopeId(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function setMessageDeliveryStatusByEnvelopeId(aMmdb, aEnvelopeId, aReceiver,
                                              aDeliveryStatus) {
  return callMmdbMethod(aMmdb, "setMessageDeliveryStatusByEnvelopeId",
                        aMmdb, aEnvelopeId, aReceiver, aDeliveryStatus);
}

/**
 * A convenient function for calling
 * |mmdb.setMessageReadStatusByEnvelopeId(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function setMessageReadStatusByEnvelopeId(aMmdb, aEnvelopeId, aReceiver,
                                          aReadStatus) {
  return callMmdbMethod(aMmdb, "setMessageReadStatusByEnvelopeId",
                        aEnvelopeId, aReceiver, aReadStatus);
}

/**
 * A convenient function for calling
 * |mmdb.getMessageRecordByTransactionId(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DB Record>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function getMessageRecordByTransactionId(aMmdb, aTransactionId) {
  return callMmdbMethod(aMmdb, "getMessageRecordByTransactionId",
                        aTransactionId);
}

/**
 * A convenient function for calling |mmdb.getMessageRecordById(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <DB Record>, <DOM message>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function getMessageRecordById(aMmdb, aMessageId) {
  return callMmdbMethod(aMmdb, "getMessageRecordById", aMessageId);
}

/**
 * A convenient function for calling |mmdb.markMessageRead(...)|.
 *
 * Fulfill params: Ci.nsIMobileMessageCallback.FOO.
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function markMessageRead(aMmdb, aMessageId, aRead) {
  let deferred = Promise.defer();

  aMmdb.markMessageRead(aMessageId, aRead, false, {
    notifyMarkMessageReadFailed: function(aRv) {
      ok(true, "markMessageRead returns a unsuccessful code: " + aRv);
      deferred.reject(aRv);
    },

    notifyMessageMarkedRead: function(aRead) {
      ok(true, "markMessageRead returns a successful code: " + Cr.NS_OK);
      deferred.resolve(Ci.nsIMobileMessageCallback.SUCCESS_NO_ERROR);
    }
  });

  return deferred.promise;
}

/**
 * A convenient function for calling |mmdb.deleteMessage(...)|.
 *
 * Fulfill params: array of deleted flags.
 * Reject params: Ci.nsIMobileMessageCallback.FOO.
 *
 * @return A deferred promise.
 */
function deleteMessage(aMmdb, aMessageIds, aLength) {
  let deferred = Promise.defer();

  aMmdb.deleteMessage(aMessageIds, aLength, {
    notifyDeleteMessageFailed: function(aRv) {
      ok(true, "deleteMessage returns a unsuccessful code: " + aRv);
      deferred.reject(aRv);
    },

    notifyMessageDeleted: function(aDeleted, aLength) {
      ok(true, "deleteMessage successfully!");
      deferred.resolve(aDeleted);
    }
  });

  return deferred.promise;
}

/**
 * A convenient function for calling |mmdb.saveSmsSegment(...)|.
 *
 * Fulfill params: [<Cr.NS_ERROR_FOO>, <completeMessage>].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function saveSmsSegment(aMmdb, aSmsSegment) {
  return callMmdbMethod(aMmdb, "saveSmsSegment", aSmsSegment);
}

/**
 * Utility function for calling cursor-based MMDB methods.
 *
 * Resolve when the target method notifies us with |notifyCursorDone|,
 * reject otherwise.
 *
 * Fulfill params: [<Ci.nsIMobileMessageCallback.FOO>, [<DOM message/thread>]]
 * Reject params: same as fulfill params.
 *
 * @param aMmdb
 *        A MobileMessageDB instance.
 * @param aMethodName
 *        A string name for that target method.
 * @param ...
 *        Extra arguments to pass to that target method. The last callback
 *        argument should always be excluded.
 *
 * @return A deferred promise.
 */
function createMmdbCursor(aMmdb, aMethodName) {
  let deferred = Promise.defer();

  let cursor;
  let results = [];
  let args = Array.slice(arguments, 2);
  args.push({
    notifyCursorError: function(aRv) {
      ok(true, "notifyCursorError: " + aRv);
      deferred.reject([aRv, results]);
    },

    notifyCursorResult: function(aResults, aSize) {
      ok(true, "notifyCursorResult: " + aResults.map(function(aElement) { return aElement.id; }));
      results = results.concat(aResults);
      cursor.handleContinue();
    },

    notifyCursorDone: function() {
      ok(true, "notifyCursorDone");
      deferred.resolve([Ci.nsIMobileMessageCallback.SUCCESS_NO_ERROR, results]);
    }
  });

  cursor = aMmdb[aMethodName].apply(aMmdb, args);

  return deferred.promise;
}

/**
 * A convenient function for calling |mmdb.createMessageCursor(...)|.
 *
 * Fulfill params: [<Ci.nsIMobileMessageCallback.FOO>, [<DOM message>]].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function createMessageCursor(aMmdb, aStartDate = null, aEndDate = null,
                             aNumbers = null, aDelivery = null, aRead = null,
                             aThreadId = null, aReverse = false) {
  return createMmdbCursor(aMmdb, "createMessageCursor",
                          aStartDate !== null,
                          aStartDate || 0,
                          aEndDate !== null,
                          aEndDate || 0,
                          aNumbers || null,
                          aNumbers && aNumbers.length || 0,
                          aDelivery || null,
                          aRead !== null,
                          aRead || false,
                          aThreadId !== null,
                          aThreadId || 0,
                          aReverse || false);
}

/**
 * A convenient function for calling |mmdb.createThreadCursor(...)|.
 *
 * Fulfill params: [<Ci.nsIMobileMessageCallback.FOO>, [<DOM thread>]].
 * Reject params: same as fulfill params.
 *
 * @return A deferred promise.
 */
function createThreadCursor(aMmdb) {
  return createMmdbCursor(aMmdb, "createThreadCursor");
}

// A reference to a nsIUUIDGenerator service.
var _uuidGenerator;

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
