/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 180000;
MARIONETTE_HEAD_JS = 'head.js';

/******************************************************************************
 ****                          Basic Operations                            ****
 ******************************************************************************/

// In a CDMA network, a modem can connected with at most 2 remote parties at a
// time. To align to the spec defined in 3gpp2-Call Waiting, in this testcase, A
// refers to local party, while B and C refer to the first and the second remote
// party respectively.

let Numbers = ["0911111111",
               "0922222222"];

function exptectedCall(aCall, aNumberIndex, aState, aEmulatorState = null) {
  let disconnectedReason = (aState === "disconnected") ? "NormalCallClearing"
                                                       : null;

  return TelephonyHelper.createExptectedCall(aCall, Numbers[aNumberIndex],
                                             false, "in", aState,
                                             aEmulatorState,
                                             disconnectedReason);
}

function empty(aCalls) {
  return [exptectedCall(aCalls[0], 0, "disconnected"),
          exptectedCall(aCalls[1], 1, "disconnected")];
}

function flash(aCall) {
  return TelephonyHelper.hold(aCall, false);
}

/******************************************************************************
 ****                               Plots                                  ****
 ******************************************************************************/

/* State defined by 3GPP2          | Abbreviation   | Remote call state
   --------------------------------+----------------+------------------------
   2-Way                           | N/A            | [connected]
   --------------------------------+----------------+------------------------
   2-Way Call Waiting Notification | CWN            | [connected, waiting]
   --------------------------------+----------------+------------------------
   2-Way Call Waiting              | CW             | [connected, held],
                                   |                | [held, connected]       */

let Opening = (function() {

  function CWN() {
    let call;
    return Promise.resolve()
      .then(() => Remote.dial(Numbers[0])).then(aCall => call = aCall)
      .then(() => TelephonyHelper.equals([exptectedCall(call, 0, "incoming")]))

      .then(() => TelephonyHelper.answer(call))
      .then(() => TelephonyHelper.equals([exptectedCall(call, 0, "connected")]))

      .then(() => Remote.dial(Numbers[1]))
      .then(() => {
        let calls = [exptectedCall(call, 0, "connected", "active"),
                     exptectedCall(call, 1, "connected", "waiting")];
        return TelephonyHelper.equals(calls);
      })

      // Return the call
      .then(() => call);
  }

  function CW() {
    let call;
    return CWN()
      .then(aCall => call = aCall)
      .then(() => flash(call))
      .then(() => {
        // After A-C connection is constructed, A-C becomes the active connection.
        let calls = [exptectedCall(call, 0, "connected", "held"),
                     exptectedCall(call, 1, "connected", "active")];
        return TelephonyHelper.equals(calls);
      })

      // Return the call
      .then(() => call);
  }

  function CW_1Flash() {
    let call;
    return CW()
      .then(aCall => call = aCall)
      .then(() => flash(call))
      .then(() => {
        let calls = [exptectedCall(call, 0, "connected", "active"),
                     exptectedCall(call, 1, "connected", "held")];
        return TelephonyHelper.equals(calls);
      })

      // Return the call
      .then(() => call);
  }

  function CW_2Flash() {
    let call;
    return CW_1Flash()
      .then(aCall => call = aCall)
      .then(() => flash(call))
      .then(() => {
        let calls = [exptectedCall(call, 0, "connected", "held"),
                     exptectedCall(call, 1, "connected", "active")];
        return TelephonyHelper.equals(calls);
      })

      // Return the call
      .then(() => call);
  }

  function CW_3Flash() {
    let call;
    return CW_2Flash()
      .then(aCall => call = aCall)
      .then(() => flash(call))
      .then(() => {
        let calls = [exptectedCall(call, 0, "connected", "active"),
                     exptectedCall(call, 1, "connected", "held")];
        return TelephonyHelper.equals(calls);
      })

      // Return the call
      .then(() => call);
  }

  return {
    CWN: CWN,
    CW:  CW,
    CW_1Flash: CW_1Flash,
    CW_2Flash: CW_2Flash,
    CW_3Flash: CW_3Flash
  };
})();

let Ending = (function() {
  // No matter how many remote parties are connected, locally hanging up the
  // call will disconnect every remote party.
  function AHangUp(aCall) {
    return Promise.resolve()
      .then(() => TelephonyHelper.hangUp(aCall))
      .then(() => TelephonyHelper.equals(empty([aCall, aCall])));
  }

  // B is the only remote party now, and B is going to hang up.
  function BHangUp(aCall) {
    return Promise.resolve()
      .then(() => Remote.hangUp(Numbers[0], true))
      .then(() => TelephonyHelper.equals(empty([aCall, aCall])));
  }

  // C is the only remote party now, and C is going to hang up.
  function CHangUp(aCall) {
    return Promise.resolve()
      .then(() => Remote.hangUp(Numbers[1], true))
      .then(() => TelephonyHelper.equals(empty([aCall, aCall])));
  }

  // Both B and C are connected now, and B is going to hang up.
  function intermediate_BHangUp(aCall) {
    // After B hang up, A-C connection will become active. On the other hand,
    // since there is no notification for the disconnection of B, we still think
    // B is connected.
    let calls = [exptectedCall(aCall, 0, "connected", "disconnected"),
                 exptectedCall(aCall, 1, "connected", "active")];

    return Promise.resolve()
      .then(() => Remote.hangUp(Numbers[0], false))
      .then(() => TelephonyHelper.equals(calls))
      .then(() => aCall);
  }

  // Both B and C are connected now, and C is going to hang up.
  function intermediate_CHangUp(aCall) {
    // After C hang up, A-C connection will become active. On the other hand,
    // since there is no notification for the disconnection of C, we still think
    // C is connected.
    let calls = [exptectedCall(aCall, 0, "connected", "active"),
                 exptectedCall(aCall, 1, "connected", "disconnected")];

    return Promise.resolve()
      .then(() => Remote.hangUp(Numbers[1], false))
      .then(() => TelephonyHelper.equals(calls))
      .then(() => aCall);
  }

  function BHangUp_AHangUp(aCall) {
    return intermediate_BHangUp(aCall).then(aCall => AHangUp(aCall));
  }

  function BHangUp_CHangUp(aCall) {
    return intermediate_BHangUp(aCall).then(aCall => CHangUp(aCall));
  }

  function CHangUp_AHangUp(aCall) {
    return intermediate_CHangUp(aCall).then(aCall => AHangUp(aCall));
  }

  function CHangUp_BHangUp(aCall) {
    return intermediate_CHangUp(aCall).then(aCall => BHangUp(aCall));
  }

  return {
    AHangUp: AHangUp,
    BHangUp: BHangUp,
    CHangUp: CHangUp,

    BHangUp_AHangUp: BHangUp_AHangUp,
    BHangUp_CHangUp: BHangUp_CHangUp,
    CHangUp_AHangUp: CHangUp_AHangUp,
    CHangUp_BHangUp: CHangUp_BHangUp
  };
})();

/******************************************************************************
 ****                             Testcases                                ****
 ******************************************************************************/

// Tests for call waiting nofication state. [active, waiting]
function runTestSuiteForCallWaitingNotification() {
  let endings = [Ending.AHangUp,
                 Ending.BHangUp,
                 Ending.CHangUp_AHangUp,
                 Ending.CHangUp_BHangUp];

  let promise = Promise.resolve();
  endings.forEach(ending => {
    promise = promise
      .then(() => log("= Test: " + Opening.CWN.name + "_" + ending.name + " ="))
      .then(() => Opening.CWN())
      .then(aCall => ending(aCall));
  });

  return promise;
}

// Tests for call waiting state. [active, held] or [held, active]
function runTestSuiteForCallWaiting() {
  let openings = [Opening.CW,
                  Opening.CW_1Flash,
                  Opening.CW_2Flash,
                  Opening.CW_3Flash];

  let endings = [Ending.AHangUp,
                 Ending.BHangUp_AHangUp,
                 Ending.BHangUp_CHangUp,
                 Ending.CHangUp_AHangUp,
                 Ending.CHangUp_BHangUp];

  let promise = Promise.resolve();
  openings.forEach(opening => {
    endings.forEach(ending => {
      promise = promise
        .then(() => log("= Test: " + opening.name + "_" + ending.name + " ="))
        .then(() => opening())
        .then(aCall => ending(aCall));
    });
  });

  return promise;
}

/******************************************************************************
 ****                           Test Launcher                              ****
 ******************************************************************************/

startTest(function() {
  return Promise.resolve()
    // Setup Environment
    .then(() => Modem.changeTech("cdma"))

    .then(() => runTestSuiteForCallWaitingNotification())
    .then(() => runTestSuiteForCallWaiting())

    // Restore Environment
    .then(() => Modem.changeTech("wcdma"))

    // Finalize
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
