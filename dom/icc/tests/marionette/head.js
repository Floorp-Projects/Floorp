/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const PREF_KEY_RIL_DEBUGGING_ENABLED = "ril.debugging.enabled";

// The pin code hard coded in emulator is "0000".
const DEFAULT_PIN = "0000";

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
