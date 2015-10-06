/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

/******************************************************************************
 ****                          Basic Operations                            ****
 ******************************************************************************/

const OutgoingNumber = "5555551111";

function exptectedCall(aCall, aState, aEmulatorState = null) {
  let disconnectedReason = aState === "disconnected" ? "NormalCallClearing"
                                                     : null;

  return TelephonyHelper.createExptectedCall(aCall, OutgoingNumber, false,
                                             "out", aState, aEmulatorState,
                                             disconnectedReason);
}

function outgoing(aNumber) {
  let ret;

  // Since a CDMA call doesn't have "alerting" state, it directly goes to
  // "connected" state instead.
  let state = Modem.isCDMA() ? "connected" : "alerting";
  return TelephonyHelper.dial(aNumber)
    .then(call => ret = call)
    .then(() => TelephonyHelper.equals([exptectedCall(ret, state)]))
    .then(() => ret);
}

function remoteAnswer(aCall) {
  let call = exptectedCall(aCall, "connected");
  return Remote.answer(aCall)
    .then(() => TelephonyHelper.equals([call]))
    .then(() => aCall);
}

function hold(aCall) {
  // Since a CDMA call doesn't have any notification for its state changing to
  // "held" state, a telephonyCall is still remains in "connected" state when
  // the call actually goes to "held" state, and we shouldn't wait for the state
  // change event here.
  let state = Modem.isGSM() ? "held" : "connected";
  let call = exptectedCall(aCall, state, "held");
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

function testOutgoingReject() {
  log("= testOutgoingReject =");
  return outgoing(OutgoingNumber)
    .then(call => remoteHangUp(call));
}

function testOutgoingCancel() {
  log("= testOutgoingCancel =");
  return outgoing(OutgoingNumber)
    .then(call => hangUp(call));
}

function testOutgoingAnswerHangUp() {
  log("= testOutgoingAnswerHangUp =");
  return outgoing(OutgoingNumber)
    .then(call => remoteAnswer(call))
    .then(call => hangUp(call));
}

function testOutgoingAnswerRemoteHangUp() {
  log("= testOutgoingAnswerRemoteHangUp =");
  return outgoing(OutgoingNumber)
    .then(call => remoteAnswer(call))
    .then(call => remoteHangUp(call));
}

function testOutgoingAnswerHoldHangUp() {
  log("= testOutgoingAnswerHoldHangUp =");
  return outgoing(OutgoingNumber)
    .then(call => remoteAnswer(call))
    .then(call => hold(call))
    .then(call => hangUp(call));
}

function testOutgoingAnswerHoldRemoteHangUp() {
  log("= testOutgoingAnswerHoldRemoteHangUp =");
  return outgoing(OutgoingNumber)
    .then(call => remoteAnswer(call))
    .then(call => hold(call))
    .then(call => remoteHangUp(call));
}

function testOutgoingAnswerHoldResumeHangUp() {
  log("= testOutgoingAnswerHoldResumeHangUp =");
  return outgoing(OutgoingNumber)
    .then(call => remoteAnswer(call))
    .then(call => hold(call))
    .then(call => resume(call))
    .then(call => hangUp(call));
}

function testOutgoingAnswerHoldResumeRemoteHangUp() {
  log("= testOutgoingAnswerHoldResumeRemoteHangUp =");
  return outgoing(OutgoingNumber)
    .then(call => remoteAnswer(call))
    .then(call => hold(call))
    .then(call => resume(call))
    .then(call => remoteHangUp(call));
}

/******************************************************************************/
/***                            Test Launcher                               ***/
/******************************************************************************/

function runTestSuite(aTech, aTechMask) {
  return Promise.resolve()
    // Setup Environment
    .then(() => Modem.changeTech(aTech, aTechMask))

    // Tests
    .then(() => testOutgoingReject())
    .then(() => testOutgoingCancel())
    .then(() => testOutgoingAnswerHangUp())
    .then(() => testOutgoingAnswerRemoteHangUp())
    .then(() => testOutgoingAnswerHoldHangUp())
    .then(() => testOutgoingAnswerHoldRemoteHangUp())
    .then(() => testOutgoingAnswerHoldResumeHangUp())
    .then(() => testOutgoingAnswerHoldResumeRemoteHangUp())

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

