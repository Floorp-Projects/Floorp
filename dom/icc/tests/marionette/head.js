/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {Cc: Cc, Ci: Ci, Cr: Cr, Cu: Cu} = SpecialPowers;

const PREF_KEY_RIL_DEBUGGING_ENABLED = "ril.debugging.enabled";

// The pin code hard coded in emulator is "0000".
const DEFAULT_PIN = "0000";
// The puk code hard coded in emulator is "12345678".
const DEFAULT_PUK = "12345678";

const WHT = 0xFFFFFFFF;
const BLK = 0x000000FF;
const RED = 0xFF0000FF;
const GRN = 0x00FF00FF;
const BLU = 0x0000FFFF;
const TSP = 0;

// Basic Image, see record number 1 in EFimg.
const BASIC_ICON = {
  width: 8,
  height: 8,
  codingScheme: "basic",
  pixels: [WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,
           BLK, BLK, BLK, BLK, BLK, BLK, WHT, WHT,
           WHT, BLK, WHT, BLK, BLK, WHT, BLK, WHT,
           WHT, BLK, BLK, WHT, WHT, BLK, BLK, WHT,
           WHT, BLK, BLK, WHT, WHT, BLK, BLK, WHT,
           WHT, BLK, WHT, BLK, BLK, WHT, BLK, WHT,
           WHT, WHT, BLK, BLK, BLK, BLK, WHT, WHT,
           WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT]
};
// Color Image, see record number 3 in EFimg.
const COLOR_ICON = {
  width: 8,
  height: 8,
  codingScheme: "color",
  pixels: [BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU,
           BLU, RED, RED, RED, RED, RED, RED, BLU,
           BLU, RED, GRN, GRN, GRN, RED, RED, BLU,
           BLU, RED, RED, GRN, GRN, RED, RED, BLU,
           BLU, RED, RED, GRN, GRN, RED, RED, BLU,
           BLU, RED, RED, GRN, GRN, GRN, RED, BLU,
           BLU, RED, RED, RED, RED, RED, RED, BLU,
           BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU]
};
// Color Image with Transparency, see record number 5 in EFimg.
const COLOR_TRANSPARENCY_ICON = {
  width: 8,
  height: 8,
  codingScheme: "color-transparency",
  pixels: [TSP, TSP, TSP, TSP, TSP, TSP, TSP, TSP,
           TSP, RED, RED, RED, RED, RED, RED, TSP,
           TSP, RED, GRN, GRN, GRN, RED, RED, TSP,
           TSP, RED, RED, GRN, GRN, RED, RED, TSP,
           TSP, RED, RED, GRN, GRN, RED, RED, TSP,
           TSP, RED, RED, GRN, GRN, GRN, RED, TSP,
           TSP, RED, RED, RED, RED, RED, RED, TSP,
           TSP, TSP, TSP, TSP, TSP, TSP, TSP, TSP]
};

/**
 * Helper function for checking stk icon.
 */
function isIcons(aIcons, aExpectedIcons) {
  is(aIcons.length, aExpectedIcons.length, "icons.length");
  for (let i = 0; i < aIcons.length; i++) {
    let icon = aIcons[i];
    let expectedIcon = aExpectedIcons[i];

    is(icon.width, expectedIcon.width, "icon.width");
    is(icon.height, expectedIcon.height, "icon.height");
    is(icon.codingScheme, expectedIcon.codingScheme, "icon.codingScheme");

    is(icon.pixels.length, expectedIcon.pixels.length);
    for (let j = 0; j < icon.pixels.length; j++) {
      is(icon.pixels[j], expectedIcon.pixels[j], "icon.pixels[" + j + "]");
    }
  }
}

/**
 * Helper function for checking stk text.
 */
function isStkText(aStkText, aExpectedStkText) {
  is(aStkText.text, aExpectedStkText.text, "stkText.text");
  if (aExpectedStkText.icons) {
    is(aStkText.iconSelfExplanatory, aExpectedStkText.iconSelfExplanatory,
       "stkText.iconSelfExplanatory");
    isIcons(aStkText.icons, aExpectedStkText.icons);
  }
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
  return new Promise(function(aResolve, aReject) {
    ++_pendingEmulatorCmdCount;
    runEmulatorCmd(aCommand, function(aResult) {
      --_pendingEmulatorCmdCount;

      ok(true, "Emulator response: " + JSON.stringify(aResult));
      if (Array.isArray(aResult) &&
          aResult[aResult.length - 1] === "OK") {
        aResolve(aResult);
      } else {
        aReject(aResult);
      }
    });
  });
}

/**
 * Send stk proactive pdu.
 *
 * Fulfill params:
 *   result -- an array of emulator response lines.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aPdu
 *
 * @return A deferred promise.
 */
function sendEmulatorStkPdu(aPdu) {
  let cmd = "stk pdu " + aPdu;
  return runEmulatorCmdSafe(cmd);
}

/**
 * Peek the last STK terminal response sent to modem.
 *
 * Fulfill params:
 *   result -- last terminal response in HEX String.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function peekLastStkResponse() {
  return runEmulatorCmdSafe("stk lastresponse")
    .then(aResult => aResult[0]);
}

/**
 * Peek the last STK envelope sent to modem.
 *
 * Fulfill params:
 *   result -- last envelope in HEX String.
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @return A deferred promise.
 */
function peekLastStkEnvelope() {
  return runEmulatorCmdSafe("stk lastenvelope")
    .then(aResult => aResult[0]);
}

/**
 * Verify with the peeked STK response.
 *
 * Fulfill params:
 *   result -- (none)
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aExpectResponse
 *        The expected Response PDU in HEX String.
 *
 * @return A deferred promise.
 */
function verifyWithPeekedStkResponse(aExpectResponse) {
  return new Promise(function(aResolve, aReject) {
    window.setTimeout(function() {
      peekLastStkResponse().then(aResult => {
        is(aResult, aExpectResponse, "Verify sent APDU");
        aResolve();
      });
    }, 3000);
  });
}

/**
 * Verify with the peeked STK response.
 *
 * Fulfill params:
 *   result -- (none)
 * Reject params:
 *   result -- an array of emulator response lines.
 *
 * @param aExpectEnvelope
 *        The expected Envelope PDU in HEX String.
 *
 * @return A deferred promise.
 */
function verifyWithPeekedStkEnvelope(aExpectEnvelope) {
  return new Promise(function(aResolve, aReject) {
    window.setTimeout(function() {
      peekLastStkEnvelope().then(aResult => {
        is(aResult, aExpectEnvelope, "Verify sent APDU");
        aResolve();
      });
    }, 3000);
  });
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
  return new Promise(function(aResolve, aReject) {
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
          aResolve(iccManager);
        } else {
          aReject();
        }
      });

      document.body.appendChild(workingFrame);
    });
  });
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
  return new Promise(function(aResolve, aReject) {
    aEventTarget.addEventListener(aEventName, function onevent(aEvent) {
      if (!aMatchFun || aMatchFun(aEvent)) {
        aEventTarget.removeEventListener(aEventName, onevent);
        ok(true, "Event '" + aEventName + "' got.");
        aResolve(aEvent);
      }
    });
  });
}

/**
 * Wait for one named system message.
 *
 * Resolve if that named message is received. Never reject.
 *
 * Fulfill params: the message passed.
 *
 * @param aEventName
 *        A string message name.
 * @param aMatchFun [optional]
 *        A matching function returns true or false to filter the message. If no
 *        matching function passed the promise is resolved after receiving the
 *        first message.
 *
 * @return A deferred promise.
 */
function waitForSystemMessage(aMessageName, aMatchFun) {
  let target = workingFrame.contentWindow.navigator;

  return new Promise(function(aResolve, aReject) {
    target.mozSetMessageHandler(aMessageName, function(aMessage) {
      if (!aMatchFun || aMatchFun(aMessage)) {
        target.mozSetMessageHandler(aMessageName, null);
        ok(true, "System message '" + aMessageName + "' got.");
        aResolve(aMessage);
      }
    });
  });
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
