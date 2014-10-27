/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const PREF_KEY_RIL_DEBUGGING_ENABLED = "ril.debugging.enabled";

// The pin code hard coded in emulator is "0000".
const DEFAULT_PIN = "0000";
// The puk code hard coded in emulator is "12345678".
const DEFAULT_PUK = "12345678";

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); }
function Deferred() {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

let _pendingEmulatorCmdCount = 0;

/**
 * Send emulator command with safe guard.
 *
 * We should only call |finish()| after all emulator command transactions
 * end, so here comes with the pending counter.  Resolve when the emulator
 * gives positive response, and reject otherwise.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aCommand
 *        A string command to be passed to emulator through its telnet console.
 *
 * @return A deferred promise.
 */
function runEmulatorCmdSafe(aCommand) {
  let deferred = Promise.defer();

  ++_pendingEmulatorCmdCount;
  runEmulatorCmd(aCommand, function(aResult) {
    --_pendingEmulatorCmdCount;

    ok(true, "Emulator response: " + JSON.stringify(aResult));
    if (Array.isArray(aResult) &&
        aResult[aResult.length - 1] === "OK") {
      deferred.resolve(aResult);
    } else {
      deferred.reject(aResult);
    }
  });

  return deferred.promise;
}

let workingFrame;
let iccManager;

/**
 * Push required permissions and test if
 * |navigator.mozIccManager| exists. Resolve if it does,
 * reject otherwise.
 *
 * Fulfill params:
 *   iccManager -- an reference to navigator.mozIccManager.
 *
 * Reject params: (none)
 *
 * @param aAdditonalPermissions [optional]
 *        An array of permission strings other than "mobileconnection" to be
 *        pushed. Default: empty string.
 *
 * @return A deferred promise.
 */
function ensureIccManager(aAdditionalPermissions) {
  let deferred = Promise.defer();

  aAdditionalPermissions = aAdditionalPermissions || [];

  if (aAdditionalPermissions.indexOf("mobileconnection") < 0) {
    aAdditionalPermissions.push("mobileconnection");
  }
  let permissions = [];
  for (let perm of aAdditionalPermissions) {
    permissions.push({ "type": perm, "allow": 1, "context": document });
  }

  SpecialPowers.pushPermissions(permissions, function() {
    ok(true, "permissions pushed: " + JSON.stringify(permissions));

    // Permission changes can't change existing Navigator.prototype
    // objects, so grab our objects from a new Navigator.
    workingFrame = document.createElement("iframe");
    workingFrame.addEventListener("load", function load() {
      workingFrame.removeEventListener("load", load);

      iccManager = workingFrame.contentWindow.navigator.mozIccManager;

      if (iccManager) {
        ok(true, "navigator.mozIccManager is instance of " + iccManager.constructor);
      } else {
        ok(true, "navigator.mozIccManager is undefined");
      }

      if (iccManager instanceof MozIccManager) {
        deferred.resolve(iccManager);
      } else {
        deferred.reject();
      }
    });

    document.body.appendChild(workingFrame);
  });

  return deferred.promise;
}

/**
 * Get MozIcc by IccId
 *
 * @param aIccId [optional]
 *        Default: The first item of |aIccManager.iccIds|.
 *
 * @return A MozIcc.
 */
function getMozIcc(aIccId) {
  aIccId = aIccId || iccManager.iccIds[0];

  if (!aIccId) {
    ok(true, "iccManager.iccIds[0] is undefined");
    return null;
  }

  return iccManager.getIccById(aIccId);
}

/**
 * Get MozMobileConnection by ServiceId
 *
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: 0 if not indicated.
 *
 * @return A MozMobileConnection.
 */
function getMozMobileConnectionByServiceId(aServiceId) {
  aServiceId = aServiceId || 0;
  return workingFrame.contentWindow.navigator.mozMobileConnections[aServiceId];
}

/**
 * Set radio enabling state.
 *
 * Resolve no matter the request succeeds or fails. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id.
 *
 * @return A deferred promise.
 */
function setRadioEnabled(aEnabled, aServiceId) {
  return getMozMobileConnectionByServiceId(aServiceId).setRadioEnabled(aEnabled)
    .then(() => {
      ok(true, "setRadioEnabled " + aEnabled + " on " + aServiceId + " success.");
    }, (aError) => {
      ok(false, "setRadioEnabled " + aEnabled + " on " + aServiceId + " " +
         aError.name);
    });
}

/**
 * Wait for one named event.
 *
 * Resolve if that named event occurs.  Never reject.
 *
 * Fulfill params: the DOMEvent passed.
 *
 * @param aEventTarget
 *        An EventTarget object.
 * @param aEventName
 *        A string event name.
 * @param aMatchFun [optional]
 *        A matching function returns true or false to filter the event.
 *
 * @return A deferred promise.
 */
function waitForTargetEvent(aEventTarget, aEventName, aMatchFun) {
  let deferred = Promise.defer();

  aEventTarget.addEventListener(aEventName, function onevent(aEvent) {
    if (!aMatchFun || aMatchFun(aEvent)) {
      aEventTarget.removeEventListener(aEventName, onevent);
      ok(true, "Event '" + aEventName + "' got.");
      deferred.resolve(aEvent);
    }
  });

  return deferred.promise;
}

/**
 * Set radio enabling state and wait for "radiostatechange" event.
 *
 * Resolve if radio state changed to the expected one. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aEnabled
 *        A boolean state.
 * @param aServiceId [optional]
 *        A numeric DSDS service id. Default: the one indicated in
 *        start*TestCommon() or 0 if not indicated.
 *
 * @return A deferred promise.
 */
function setRadioEnabledAndWait(aEnabled, aServiceId) {
  let mobileConn = getMozMobileConnectionByServiceId(aServiceId);
  let promises = [];

  promises.push(waitForTargetEvent(mobileConn, "radiostatechange", function() {
    // To ignore some transient states, we only resolve that deferred promise
    // when |radioState| equals to the expected one.
    return mobileConn.radioState === (aEnabled ? "enabled" : "disabled");
  }));
  promises.push(setRadioEnabled(aEnabled, aServiceId));

  return Promise.all(promises);
}

/**
 * Restart radio and wait card state changes to expected one.
 *
 * Resolve if card state changed to the expected one. Never reject.
 *
 * Fulfill params: (none)
 *
 * @param aCardState
 *        Expected card state.
 *
 * @return A deferred promise.
 */
function restartRadioAndWait(aCardState) {
  return setRadioEnabledAndWait(false).then(() => {
    let promises = [];

    promises.push(waitForTargetEvent(iccManager, "iccdetected")
      .then((aEvent) => {
        let icc = getMozIcc(aEvent.iccId);
        if (icc.cardState !== aCardState) {
          return waitForTargetEvent(icc, "cardstatechange", function() {
            return icc.cardState === aCardState;
          });
        }
      }));
    promises.push(setRadioEnabledAndWait(true));

    return Promise.all(promises);
  });
}

/**
 * Enable/Disable PIN-lock.
 *
 * Fulfill params: (none)
 * Reject params:
 *   An object contains error name and remaining retry count.
 *   @see IccCardLockError
 *
 * @param aIcc
 *        A MozIcc object.
 * @param aEnabled
 *        A boolean state.
 *
 * @return A deferred promise.
 */
function setPinLockEnabled(aIcc, aEnabled) {
  let options = {
    lockType: "pin",
    enabled: aEnabled,
    pin: DEFAULT_PIN
  };

  return aIcc.setCardLock(options);
}

/**
 * Wait for pending emulator transactions and call |finish()|.
 */
function cleanUp() {
  // Use ok here so that we have at least one test run.
  ok(true, ":: CLEANING UP ::");

  waitFor(finish, function() {
    return _pendingEmulatorCmdCount === 0;
  });
}

/**
 * Basic test routine helper for icc tests.
 *
 * This helper does nothing but clean-ups.
 *
 * @param aTestCaseMain
 *        A function that takes no parameter.
 */
function startTestBase(aTestCaseMain) {
  // Turn on debugging pref.
  let debugPref = SpecialPowers.getBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED);
  SpecialPowers.setBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED, true);

  Promise.resolve()
    .then(aTestCaseMain)
    .catch((aError) => {
      ok(false, "promise rejects during test: " + aError);
    })
    .then(() => {
      // Restore debugging pref.
      SpecialPowers.setBoolPref(PREF_KEY_RIL_DEBUGGING_ENABLED, debugPref);
      cleanUp();
    });
}

/**
 * Common test routine helper for icc tests.
 *
 * This function ensures global variable |iccManager| and |icc| is available
 * during the process and performs clean-ups as well.
 *
 * @param aTestCaseMain
 *        A function that takes one parameter -- icc.
 * @param aAdditonalPermissions [optional]
 *        An array of permission strings other than "mobileconnection" to be
 *        pushed. Default: empty string..
 */
function startTestCommon(aTestCaseMain, aAdditionalPermissions) {
  startTestBase(function() {
    return ensureIccManager(aAdditionalPermissions)
      .then(aTestCaseMain);
  });
}
