/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

/******************************************************************************
 ****                          Basic Operations                            ****
 ******************************************************************************/

const IncomingNumber = "1111110000";

function exptectedCall(aCall, aState, aEmulatorState = null) {
  let disconnectedReason = aState === "disconnected" ? "NormalCallClearing"
                                                     : null;

  return TelephonyHelper.createExptectedCall(aCall, IncomingNumber, false,
                                             "in", aState, aEmulatorState,
                                             disconnectedReason);
}

function incoming(aNumber) {
  let ret;
  return Remote.dial(aNumber)
    .then(call => ret = call)
    .then(() => TelephonyHelper.equals([exptectedCall(ret, "incoming")]))
    .then(() => ret);
}

function answer(aCall) {
  let call = exptectedCall(aCall, "connected");
  return TelephonyHelper.answer(aCall)
    .then(() => TelephonyHelper.equals([call]))
    .then(() => aCall);
}

function hold(aCall) {
  // Since a CDMA call doesn't have any notification for its state changing to
  // "held" state, a telephonyCall is still remains in "connected" state when
  // the call actually goes to "held" state, and we shouldn't wait for the state
  // change event here.
  let state = Modem.isGSM() ? "held" : "connected";
  let call = exptectedCall(aCall, state,"held");
  return TelephonyHelper.hold(aCall, Modem.isGSM())
    .then(() => TelephonyHelper.equals([call]))
    .then(() => aCall);
}

function resume(aCall) {
  // Similar to the hold case, there is no notification for a CDMA call's state
  // change. Besides, a CDMA call still remains in "connected" state here, so we
  // have to use |hold()| function here to resume the call. Otherwise, if we use
  // |resume()| function, we'll get an invalid state error.
  let call = exptectedCall(aCall, "connected");
  return Modem.isGSM() ? TelephonyHelper.resume(aCall)
                       : TelephonyHelper.hold(aCall, false)
    .then(() => TelephonyHelper.equals([call]))
    .then(() => aCall);
}

function hangUp(aCall) {
  let call = exptectedCall(aCall, "disconnected");
  return TelephonyHelper.hangUp(aCall)
    .then(() => TelephonyHelper.equals([call]))
    .then(() => aCall);
}

function remoteHangUp(aCall) {
  let call = exptectedCall(aCall, "disconnected");
  return Remote.hangUp(aCall)
    .then(() => TelephonyHelper.equals([call]))
    .then(() => aCall);
}

/******************************************************************************
 ****                             Testcases                                ****
 ******************************************************************************/

function testIncomingReject() {
  log("= testIncomingReject =");
  return incoming(IncomingNumber)
    .then(call => hangUp(call));
}

function testIncomingCancel() {
  log("= testIncomingCancel =");
  return incoming(IncomingNumber)
    .then(call => remoteHangUp(call));
}

function testIncomingAnswerHangUp() {
  log("= testIncomingAnswerHangUp =");
  return incoming(IncomingNumber)
    .then(call => answer(call))
    .then(call => hangUp(call));
}

function testIncomingAnswerRemoteHangUp() {
  log("= testIncomingAnswerRemoteHangUp =");
  return incoming(IncomingNumber)
    .then(call => answer(call))
    .then(call => remoteHangUp(call));
}

function testIncomingAnswerHoldHangUp() {
  log("= testIncomingAnswerHoldHangUp =");
  return incoming(IncomingNumber)
    .then(call => answer(call))
    .then(call => hold(call))
    .then(call => hangUp(call));
}

function testIncomingAnswerHoldRemoteHangUp() {
  log("= testIncomingAnswerHoldRemoteHangUp =");
  return incoming(IncomingNumber)
    .then(call => answer(call))
    .then(call => hold(call))
    .then(call => remoteHangUp(call));
}

function testIncomingAnswerHoldResumeHangUp() {
  log("= testIncomingAnswerHoldResumeHangUp =");
  return incoming(IncomingNumber)
    .then(call => answer(call))
    .then(call => hold(call))
    .then(call => resume(call))
    .then(call => hangUp(call));
}

function testIncomingAnswerHoldResumeRemoteHangUp() {
  log("= testIncomingAnswerHoldResumeRemoteHangUp =");
  return incoming(IncomingNumber)
    .then(call => answer(call))
    .then(call => hold(call))
    .then(call => resume(call))
    .then(call => remoteHangUp(call));
}

/******************************************************************************
 ****                           Test Launcher                              ****
 ******************************************************************************/

function runTestSuite(aTech, aTechMask) {
  return Promise.resolve()
    // Setup Environment
    .then(() => Modem.changeTech(aTech, aTechMask))

    // Tests
    .then(() => testIncomingReject())
    .then(() => testIncomingCancel())
    .then(() => testIncomingAnswerHangUp())
    .then(() => testIncomingAnswerRemoteHangUp())
    .then(() => testIncomingAnswerHoldHangUp())
    .then(() => testIncomingAnswerHoldRemoteHangUp())
    .then(() => testIncomingAnswerHoldResumeHangUp())
    .then(() => testIncomingAnswerHoldResumeRemoteHangUp())

    // Restore Environment
    .then(() => Modem.changeTech("wcdma"));
}

startTest(function() {
  return Promise.resolve()
    .then(() => runTestSuite("cdma"))
    .then(() => runTestSuite("wcdma"))

    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});

