/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

/**
 * Push required permissions and test if |navigator.mozMobileMessage| exists.
 * Resolve if it does, reject otherwise.
 *
 * Fulfill params:
 *   manager -- an reference to navigator.mozMobileMessage.
 *
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
let manager;
function ensureMobileMessage() {
  let deferred = Promise.defer();

  let permissions = [{
    "type": "sms",
    "allow": 1,
    "context": document,
  }];
  SpecialPowers.pushPermissions(permissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    manager = window.navigator.mozMobileMessage;
    if (manager) {
      log("navigator.mozMobileMessage is instance of " + manager.constructor);
    } else {
      log("navigator.mozMobileMessage is undefined.");
    }

    if (manager instanceof MozMobileMessageManager) {
      deferred.resolve(manager);
    } else {
      deferred.reject();
    }
  });

  return deferred.promise;
}

/**
 * Send a SMS message to a single receiver.  Resolve if it succeeds, reject
 * otherwise.
 *
 * Fulfill params:
 *   message -- the sent SmsMessage.
 *
 * Reject params:
 *   error -- a DOMError.
 *
 * @param aReceiver the address of the receiver.
 * @param aText the text body of the message.
 *
 * @return A deferred promise.
 */
function sendSmsWithSuccess(aReceiver, aText) {
  let deferred = Promise.defer();

  let request = manager.send(aReceiver, aText);
  request.onsuccess = function(event) {
    deferred.resolve(event.target.result);
  };
  request.onerror = function(event) {
    deferred.reject(event.target.error);
  };

  return deferred.promise;
}

/**
 * Send a MMS message with specified parameters.  Resolve if it fails, reject
 * otherwise.
 *
 * Fulfill params:
 *   message -- the failed MmsMessage
 *
 * Reject params: (none)
 *
 * @param aMmsParameters a MmsParameters instance.
 *
 * @return A deferred promise.
 */
function sendMmsWithFailure(aMmsParameters) {
  let deferred = Promise.defer();

  manager.onfailed = function(event) {
    manager.onfailed = null;
    deferred.resolve(event.message);
  };

  let request = manager.sendMMS(aMmsParameters);
  request.onsuccess = function(event) {
    deferred.reject();
  };

  return deferred.promise;
}

/**
 * Retrieve messages from database.
 *
 * Fulfill params:
 *   messages -- an array of {Sms,Mms}Message instances.
 *
 * Reject params:
 *   event -- a DOMEvent
 *
 * @param aFilter an optional MozSmsFilter instance.
 * @param aReverse a boolean value indicating whether the order of the messages
 *                 should be reversed.
 *
 * @return A deferred promise.
 */
function getMessages(aFilter, aReverse) {
  let deferred = Promise.defer();

  if (!aFilter) {
    aFilter = new MozSmsFilter;
  }
  let messages = [];
  let cursor = manager.getMessages(aFilter, aReverse || false);
  cursor.onsuccess = function(aEvent) {
    if (cursor.result) {
      messages.push(cursor.result);
      cursor.continue();
      return;
    }

    deferred.resolve(messages);
  };
  cursor.onerror = deferred.reject.bind(deferred);

  return deferred.promise;
}

/**
 * Retrieve all messages from database.
 *
 * Fulfill params:
 *   messages -- an array of {Sms,Mms}Message instances.
 *
 * Reject params:
 *   event -- a DOMEvent
 *
 * @return A deferred promise.
 */
function getAllMessages() {
  return getMessages(null, false);
}

/**
 * Retrieve all threads from database.
 *
 * Fulfill params:
 *   threads -- an array of MozMobileMessageThread instances.
 *
 * Reject params:
 *   event -- a DOMEvent
 *
 * @return A deferred promise.
 */
function getAllThreads() {
  let deferred = Promise.defer();

  let threads = [];
  let cursor = manager.getThreads();
  cursor.onsuccess = function(aEvent) {
    if (cursor.result) {
      threads.push(cursor.result);
      cursor.continue();
      return;
    }

    deferred.resolve(threads);
  };
  cursor.onerror = deferred.reject.bind(deferred);

  return deferred.promise;
}

/**
 * Retrieve a single specified thread from database.
 *
 * Fulfill params:
 *   thread -- a MozMobileMessageThread instance.
 *
 * Reject params:
 *   event -- a DOMEvent if an error occurs in the retrieving process, or
 *            undefined if there's no such thread.
 *
 * @aThreadId a numeric value identifying the target thread.
 *
 * @return A deferred promise.
 */
function getThreadById(aThreadId) {
  return getAllThreads()
    .then(function(aThreads) {
      for (let thread of aThreads) {
        if (thread.id === aThreadId) {
          return thread;
        }
      }
      throw undefined;
    });
}

/**
 * Delete messages specified from database.
 *
 * Fulfill params:
 *   result -- an array of boolean values indicating whether delesion was
 *             actually performed on the message record with corresponding id.
 *
 * Reject params:
 *   event -- a DOMEvent.
 *
 * @aMessageId an array of numeric values identifying the target messages.
 *
 * @return An empty array if nothing to be deleted; otherwise, a deferred promise.
 */
function deleteMessagesById(aMessageIds) {
  if (!aMessageIds.length) {
    ok(true, "no message to be deleted");
    return [];
  }

  let deferred = Promise.defer();

  let request = manager.delete(aMessageIds);
  request.onsuccess = function(event) {
    deferred.resolve(event.target.result);
  };
  request.onerror = deferred.reject.bind(deferred);

  return deferred.promise;
}

/**
 * Delete messages specified from database.
 *
 * Fulfill params:
 *   result -- an array of boolean values indicating whether delesion was
 *             actually performed on the message record with corresponding id.
 *
 * Reject params:
 *   event -- a DOMEvent.
 *
 * @aMessages an array of {Sms,Mms}Message instances.
 *
 * @return A deferred promise.
 */
function deleteMessages(aMessages) {
  let ids = messagesToIds(aMessages);
  return deleteMessagesById(ids);
}

/**
 * Delete all messages from database.
 *
 * Fulfill params:
 *   ids -- an array of numeric values identifying those deleted
 *          {Sms,Mms}Messages.
 *
 * Reject params:
 *   event -- a DOMEvent.
 *
 * @return A deferred promise.
 */
function deleteAllMessages() {
  return getAllMessages().then(deleteMessages);
}

let pendingEmulatorCmdCount = 0;

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function runEmulatorCmdSafe(aCommand) {
  let deferred = Promise.defer();

  ++pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) && aResult[0] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

/**
 * Send simple text SMS to emulator.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function sendTextSmsToEmulator(aFrom, aText) {
  let command = "sms send " + aFrom + " " + aText;
  return runEmulatorCmdSafe(command);
}

/**
 * Send raw SMS TPDU to emulator.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function sendRawSmsToEmulator(aPdu) {
  let command = "sms pdu " + aPdu;
  return runEmulatorCmdSafe(command);
}

/**
 * Name space for MobileMessageDB.jsm.  Only initialized after first call to
 * newMobileMessageDB.
 */
let MMDB;

// Create a new MobileMessageDB instance.
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
 * @return The passed MobileMessageDB instance.
 */
function closeMobileMessageDB(aMmdb) {
  aMmdb.close();
  return aMmdb;
}

/**
 * Create a new array of id attribute of input messages.
 *
 * @param aMessages an array of {Sms,Mms}Message instances.
 *
 * @return an array of numeric values.
 */
function messagesToIds(aMessages) {
  let ids = [];
  for (let message of aMessages) {
    ids.push(message.id);
  }
  return ids;
}

// A reference to a nsIUUIDGenerator service.
let uuidGenerator;

/**
 * Generate a new UUID.
 *
 * @return A UUID string.
 */
function newUUID() {
  if (!uuidGenerator) {
    uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                    .getService(Ci.nsIUUIDGenerator);
    ok(uuidGenerator, "uuidGenerator");
  }

  return uuidGenerator.generateUUID().toString();
}

/**
 * Flush permission settings and call |finish()|.
 */
function cleanUp() {
  waitFor(function() {
    SpecialPowers.flushPermissions(function() {
      // Use ok here so that we have at least one test run.
      ok(true, "permissions flushed");

      finish();
    });
  }, function() {
    return pendingEmulatorCmdCount === 0;
  });
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
         .then(cleanUp, function() {
           ok(false, 'promise rejects during test.');
           cleanUp();
         });
}

/**
 * Common test routine helper for mobile message tests.
 *
 * This function ensures global |manager| variable is available during the
 * process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestCommon(aTestCaseMain) {
  startTestBase(function() {
    return ensureMobileMessage()
      .then(deleteAllMessages)
      .then(aTestCaseMain)
      .then(deleteAllMessages);
  });
}
