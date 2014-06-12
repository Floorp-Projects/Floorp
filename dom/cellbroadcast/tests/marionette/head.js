/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

let Promise = Cu.import("resource://gre/modules/Promise.jsm").Promise;

/**
 * Push required permissions and test if |navigator.mozCellBroadcast| exists.
 * Resolve if it does, reject otherwise.
 *
 * Fulfill params:
 *   cbManager -- an reference to navigator.mozCellBroadcast.
 *
 * Reject params: (none)
 *
 * @return A deferred promise.
 */
let cbManager;
function ensureCellBroadcast() {
  let deferred = Promise.defer();

  let permissions = [{
    "type": "cellbroadcast",
    "allow": 1,
    "context": document,
  }];
  SpecialPowers.pushPermissions(permissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    cbManager = window.navigator.mozCellBroadcast;
    if (cbManager) {
      log("navigator.mozCellBroadcast is instance of " + cbManager.constructor);
    } else {
      log("navigator.mozCellBroadcast is undefined.");
    }

    if (cbManager instanceof window.MozCellBroadcast) {
      deferred.resolve(cbManager);
    } else {
      deferred.reject();
    }
  });

  return deferred.promise;
}

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
let pendingEmulatorCmdCount = 0;
function runEmulatorCmdSafe(aCommand) {
  let deferred = Promise.defer();

  ++pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) && aResult[aResult.length - 1] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

/**
 * Send raw CBS PDU to emulator.
 *
 * @param: aPdu
 *         A hex string representing the whole CBS PDU.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function sendRawCbsToEmulator(aPdu) {
  let command = "cbs pdu " + aPdu;
  return runEmulatorCmdSafe(command);
}

/**
 * Wait for one named Cellbroadcast event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventName
 *        A string event name.
 *
 * @return A deferred promise.
 */
function waitForManagerEvent(aEventName) {
  let deferred = Promise.defer();

  cbManager.addEventListener(aEventName, function onevent(aEvent) {
    cbManager.removeEventListener(aEventName, onevent);

    ok(true, "Cellbroadcast event '" + aEventName + "' got.");
    deferred.resolve(aEvent);
  });

  return deferred.promise;
}

/**
 * Send multiple raw CB PDU to emulator and wait
 *
 * @param: aPdus
 *         A array of hex strings. Each represents a CB PDU.
 *         These PDUs are expected to be concatenated into single CB Message.
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
function sendMultipleRawCbsToEmulatorAndWait(aPdus) {
  let promises = [];

  promises.push(waitForManagerEvent("received"));
  for (let pdu of aPdus) {
    promises.push(sendRawCbsToEmulator(pdu));
  }

  return Promise.all(promises);
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
 * Switch modem for receving upcoming emulator commands.
 *
 * @param: aServiceId
 *         The id of the modem to be switched to.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 *
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function selectModem(aServiceId) {
  let command = "mux modem " + aServiceId;
  return runEmulatorCmdSafe(command);
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
 * Common test routine helper for cell broadcast tests.
 *
 * This function ensures global |cbManager| variable is available during the
 * process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestCommon(aTestCaseMain) {
  Promise.resolve()
         .then(ensureCellBroadcast)
         .then(aTestCaseMain)
         .then(cleanUp, function() {
           ok(false, 'promise rejects during test.');
           cleanUp();
         });
}
