/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); }
function Deferred()  {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

/**
 * Push a list of preference settings. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aPrefs
 *        An JS object.  For example:
 *
 *          {'set': [['foo.bar', 2], ['magic.pref', 'baz']],
 *           'clear': [['clear.this'], ['also.this']] };
 *
 * @return A deferred promise.
 */
function pushPrefEnv(aPrefs) {
  let deferred = Promise.defer();

  SpecialPowers.pushPrefEnv(aPrefs, function() {
    ok(true, "preferences pushed: " + JSON.stringify(aPrefs));
    deferred.resolve();
  });

  return deferred.promise;
}

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
var manager;
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
 * Push required permissions and test if |navigator.mozMobileConnections| exists.
 * Resolve if it does, reject otherwise.
 *
 * Fulfill params:
 *   manager -- an reference to navigator.mozMobileConnections.
 *
 * Reject params: (none)
 *
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: 0.
 *
 * @return A deferred promise.
 */
var mobileConnection;
function ensureMobileConnection(aServiceId) {
  return new Promise(function(resolve, reject) {
    let permissions = [{
      "type": "mobileconnection",
      "allow": 1,
      "context": document,
    }];
    SpecialPowers.pushPermissions(permissions, function() {
      ok(true, "permissions pushed: " + JSON.stringify(permissions));

      let serviceId = aServiceId || 0;
      mobileConnection = window.navigator.mozMobileConnections[serviceId];
      if (mobileConnection) {
        log("navigator.mozMobileConnections[" + serviceId + "] is instance of " +
            mobileConnection.constructor);
      } else {
        log("navigator.mozMobileConnections[" + serviceId + "] is undefined");
      }

      if (mobileConnection instanceof MozMobileConnection) {
        resolve(mobileConnection);
      } else {
        reject();
      }
    });
  });
}

/**
 * Wait for one named MobileMessageManager event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventName
 *        A string event name.
 * @param aMatchFunc [optional]
 *        An additional callback function to match the interested event
 *        before removing the listener and going to resolve the promise.
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName, aMatchFunc) {
  let deferred = Promise.defer();

  manager.addEventListener(aEventName, function onevent(aEvent) {
    if (aMatchFunc && !aMatchFunc(aEvent)) {
      ok(true, "MobileMessageManager event '" + aEventName + "' got" +
               " but is not interested.");
      return;
    }

    ok(true, "MobileMessageManager event '" + aEventName + "' got.");
    manager.removeEventListener(aEventName, onevent);
    deferred.resolve(aEvent);
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
  return manager.send(aReceiver, aText);
}

/**
 * Send a SMS message to a single receiver.
 * Resolve if it fails, reject otherwise.
 *
 * Fulfill params:
 *   {
 *     message,  -- the failed MmsMessage
 *     error,    -- error of the send request
 *   }
 *
 * Reject params: (none)
 *
 * @param aReceiver the address of the receiver.
 * @param aText the text body of the message.
 *
 * @return A deferred promise.
 */
function sendSmsWithFailure(aReceiver, aText) {
  let promises = [];
  promises.push(waitForManagerEvent("failed")
    .then((aEvent) => { return aEvent.message; }));
  promises.push(manager.send(aReceiver, aText)
    .then((aResult) => { throw aResult; },
          (aError) => { return aError; }));

  return Promise.all(promises)
    .then((aResults) => { return { message: aResults[0],
                                   error: aResults[1] }; });
}

/**
 * Send a MMS message with specified parameters.  Resolve if it fails, reject
 * otherwise.
 *
 * Fulfill params:
 *   {
 *     message,  -- the failed MmsMessage
 *     error,    -- error of the send request
 *   }
 *
 * Reject params: (none)
 *
 * @param aMmsParameters a MmsParameters instance.
 *
 * @param aSendParameters a MmsSendParameters instance.
 *
 * @return A deferred promise.
 */
function sendMmsWithFailure(aMmsParameters, aSendParameters) {
  let promises = [];
  promises.push(waitForManagerEvent("failed")
    .then((aEvent) => { return aEvent.message; }));
  promises.push(manager.sendMMS(aMmsParameters, aSendParameters)
    .then((aResult) => { throw aResult; },
          (aError) => { return aError; }));

  return Promise.all(promises)
    .then((aResults) => { return { message: aResults[0],
                                   error: aResults[1] }; });
}

/**
 * Retrieve message by message id.
 *
 * Fulfill params: SmsMessage
 * Reject params:
 *   event -- a DOMEvent
 *
 * @param aId
 *        A numeric message id.
 *
 * @return A deferred promise.
 */
function getMessage(aId) {
  return manager.getMessage(aId);
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
 * @param aFilter [optional]
 *        A MobileMessageFilter object.
 * @param aReverse [optional]
 *        A boolean value indicating whether the order of the message should be
 *        reversed. Default: false.
 *
 * @return A deferred promise.
 */
function getMessages(aFilter, aReverse) {
  let deferred = Promise.defer();

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

  let promises = [];
  promises.push(waitForManagerEvent("deleted"));
  promises.push(manager.delete(aMessageIds));

  return Promise.all(promises)
    .then((aResults) => {
      return { deletedInfo: aResults[0],
               deletedFlags: aResults[1] };
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

var pendingEmulatorCmdCount = 0;

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
 * @param aFrom
 *        A string-typed from address.
 * @param aText
 *        A string-typed message body.
 *
 * @return A deferred promise.
 */
function sendTextSmsToEmulator(aFrom, aText) {
  let command = "sms send " + aFrom + " " + aText;
  return runEmulatorCmdSafe(command);
}

/**
 * Send simple text SMS to emulator and wait for a received event.
 *
 * Fulfill params: SmsMessage
 * Reject params: (none)
 *
 * @param aFrom
 *        A string-typed from address.
 * @param aText
 *        A string-typed message body.
 *
 * @return A deferred promise.
 */
function sendTextSmsToEmulatorAndWait(aFrom, aText) {
  let promises = [];
  promises.push(waitForManagerEvent("received"));
  promises.push(sendTextSmsToEmulator(aFrom, aText));
  return Promise.all(promises).then(aResults => aResults[0].message);
}

/**
 * Send raw SMS TPDU to emulator.
 *
 * @param: aPdu
 *         A hex string representing the whole SMS T-PDU.
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
 * Send multiple raw SMS TPDU to emulator and wait
 *
 * @param: aPdus
 *         A array of hex strings. Each represents a SMS T-PDU.
 *
 * Fulfill params:
 *   result -- array of resolved Promise, where
 *             result[0].message representing the received message.
 *             result[1-n] represents the response of sent emulator command.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function sendMultipleRawSmsToEmulatorAndWait(aPdus) {
  let promises = [];

  promises.push(waitForManagerEvent("received"));
  for (let pdu of aPdus) {
    promises.push(sendRawSmsToEmulator(pdu));
  }

  return Promise.all(promises);
}

/**
 * Set voice state and wait for state change.
 *
 * @param aState
 *        "unregistered", "searching", "denied", "roaming", or "home".
 *
 * @return A deferred promise.
 */
function setEmulatorVoiceStateAndWait(aState) {
  let promises = [];
  promises.push(new Promise(function(resolve, reject) {
    mobileConnection.addEventListener("voicechange", function onevent(aEvent) {
      log("voicechange: connected=" + mobileConnection.voice.connected);
      mobileConnection.removeEventListener("voicechange", onevent);
      resolve(aEvent);
    })
  }));

  promises.push(runEmulatorCmdSafe("gsm voice " + aState));
  return Promise.all(promises);
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

/**
 * Convenient function to compare two SMS messages.
 */
function compareSmsMessage(aFrom, aTo) {
  const FIELDS = ["id", "threadId", "iccId", "body", "delivery",
                  "deliveryStatus", "read", "receiver", "sender",
                  "messageClass", "timestamp", "deliveryTimestamp",
                  "sentTimestamp"];

  for (let field of FIELDS) {
    is(aFrom[field], aTo[field], "message." + field);
  }
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
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

/**
 * Helper to run the test case only needed in Multi-SIM environment.
 *
 * @param  aTest
 *         A function which will be invoked w/o parameter.
 * @return a Promise object.
 */
function runIfMultiSIM(aTest) {
  let numRIL;
  try {
    numRIL = SpecialPowers.getIntPref("ril.numRadioInterfaces");
  } catch (ex) {
    numRIL = 1;  // Pref not set.
  }

  if (numRIL > 1) {
    return aTest();
  } else {
    log("Not a Multi-SIM environment. Test is skipped.");
    return Promise.resolve();
  }
}

/**
 * Helper to enable/disable connection radio state.
 *
 * @param  aConnection
 *         connection to enable / disable
 * @param  aEnabled
 *         True to enable the radio.
 * @return a Promise object.
 */
function setRadioEnabled(aConnection, aEnabled) {
  log("setRadioEnabled to " + aEnabled);

  let deferred = Promise.defer();
  let finalState = (aEnabled) ? "enabled" : "disabled";
  if (aConnection.radioState == finalState) {
    return deferred.resolve(aConnection);
  }

  aConnection.onradiostatechange = function() {
    log("Received 'radiostatechange', radioState: " + aConnection.radioState);

    if (aConnection.radioState == finalState) {
      deferred.resolve(aConnection);
      aConnection.onradiostatechange = null;
    }
  };

  let req = aConnection.setRadioEnabled(aEnabled);

  req.onsuccess = function() {
    log("setRadioEnabled success");
  };

  req.onerror = function() {
    ok(false, "setRadioEnabled should not fail");
    deferred.reject();
  };

  return deferred.promise;
}

/**
 * Helper to enable/disable all connections radio state.
 *
 * @param  aEnabled
 *         True to enable the radio.
 * @return a Promise object.
 */
function setAllRadioEnabled(aEnabled) {
  let promises = []
  for (let i = 0; i < window.navigator.mozMobileConnections.length; ++i) {
    promises.push(ensureMobileConnection(i)
      .then((connection) => setRadioEnabled(connection, aEnabled)));
  }
  return Promise.all(promises);
}
