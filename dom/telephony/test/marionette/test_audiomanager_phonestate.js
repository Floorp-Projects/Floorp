/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const AUDIO_MANAGER_CONTRACT_ID = "@mozilla.org/telephony/audiomanager;1";

// See nsIAudioManager
const PHONE_STATE_INVALID          = -2;
const PHONE_STATE_CURRENT          = -1;
const PHONE_STATE_NORMAL           = 0;
const PHONE_STATE_RINGTONE         = 1;
const PHONE_STATE_IN_CALL          = 2;
const PHONE_STATE_IN_COMMUNICATION = 3;

let conference;

function checkTelephonyActiveAndCalls(active, calls) {
  is(telephony.active, active, "telephony.active");
  is(telephony.calls.length, calls.length, "telephony.calls");
  for (let i = 0; i < calls.length; ++i) {
    is(telephony.calls[i], calls[i]);
  }
}

function checkConferenceStateAndCalls(state, calls) {
  is(conference.state, state, "conference.state");
  is(conference.calls.length, calls.length, "conference.calls");
  for (let i = 0; i < calls.length; i++) {
    is(conference.calls[i], calls[i]);
  }
}

function checkInitialState() {
  log("Verify initial state.");
  ok(telephony.calls, 'telephony.call');
  checkTelephonyActiveAndCalls(null, []);
  ok(conference.calls, 'conference.calls');
  checkConferenceStateAndCalls('', []);
}

function checkEventCallState(event, call, state) {
  is(call, event.call, "event.call");
  is(call.state, state, "call state");
}

function dial(number) {
  log("Make an outgoing call: " + number);

  let deferred = Promise.defer();

  telephony.dial(number).then(call => {
    ok(call);
    is(call.number, number);
    is(call.state, "dialing");

    call.onalerting = function onalerting(event) {
      call.onalerting = null;
      log("Received 'onalerting' call event.");
      checkEventCallState(event, call, "alerting");
      deferred.resolve(call);
    };
  });

  return deferred.promise;
}

function answer(call) {
  log("Answering the incoming call.");

  let deferred = Promise.defer();

  call.onconnecting = function onconnectingIn(event) {
    log("Received 'connecting' call event for incoming call.");
    call.onconnecting = null;
    checkEventCallState(event, call, "connecting");
  };

  call.onconnected = function onconnectedIn(event) {
    log("Received 'connected' call event for incoming call.");
    call.onconnected = null;
    checkEventCallState(event, call, "connected");
    ok(!call.onconnecting);
    deferred.resolve(call);
  };
  call.answer();

  return deferred.promise;
}

function remoteDial(number) {
  log("Simulating an incoming call.");

  let deferred = Promise.defer();

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    telephony.onimcoming = null;

    let call = event.call;

    ok(call);
    is(call.number, number);
    is(call.state, "incoming");

    deferred.resolve(call);
  };
  emulator.run("gsm call " + number);

  return deferred.promise;
}

function remoteAnswer(call) {
  log("Remote answering the call.");

  let deferred = Promise.defer();

  call.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    call.onconnected = null;
    checkEventCallState(event, call, "connected");
    deferred.resolve(call);
  };
  emulator.run("gsm accept " + call.number);

  return deferred.promise;
}

function remoteHangUp(call) {
  log("Remote hanging up the call.");

  let deferred = Promise.defer();

  call.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    call.ondisconnected = null;
    checkEventCallState(event, call, "disconnected");
    deferred.resolve(call);
  };
  emulator.run("gsm cancel " + call.number);

  return deferred.promise;
}

function remoteHangUpCalls(calls) {
  let promise = Promise.resolve();

  for (let call of calls) {
    promise = promise.then(remoteHangUp.bind(null, call));
  }

  return promise;
}

// The length of callsToAdd should be 2.
function addCallsToConference(callsToAdd) {
  log("Add " + callsToAdd.length + " calls into conference.");

  let deferred = Promise.defer();

  conference.onconnected = function() {
    deferred.resolve();
  };

  conference.add(callsToAdd[0], callsToAdd[1]);

  return deferred.promise;
}

let audioManager;
function checkStates(speakerEnabled, phoneState) {
  if (!audioManager) {
    audioManager = SpecialPowers.Cc[AUDIO_MANAGER_CONTRACT_ID]
                                .getService(SpecialPowers.Ci.nsIAudioManager);
    ok(audioManager, "nsIAudioManager instance");
  }

  is(telephony.speakerEnabled, speakerEnabled, "telephony.speakerEnabled");
  if (phoneState == PHONE_STATE_CURRENT) {
    ok(audioManager.phoneState === PHONE_STATE_CURRENT ||
       audioManager.phoneState === PHONE_STATE_NORMAL, "audioManager.phoneState");
  } else {
    is(audioManager.phoneState, phoneState, "audioManager.phoneState");
  }
}

function check(phoneStateOrig, phoneStateEnabled, phoneStateDisabled) {
  checkStates(false, phoneStateOrig);

  let canEnableSpeaker = arguments.length > 1;
  telephony.speakerEnabled = true;
  if (canEnableSpeaker) {
    checkStates(true, phoneStateEnabled);
  } else {
    checkStates(false, phoneStateOrig);
    return;
  }

  telephony.speakerEnabled = false;
  checkStates(false, arguments.length > 2 ? phoneStateDisabled : phoneStateOrig);
}

// Start the test
startTest(function() {
  conference = telephony.conferenceGroup;
  ok(conference);

  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let outCall;
  let inCall;

  Promise.resolve()
    .then(checkInitialState)
    .then(() => check(PHONE_STATE_CURRENT, PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // Dial in
    .then(() => remoteDial(inNumber))
    .then(call => { inCall = call; })
    // TODO - Bug 948860: should this be {RINGTONE, RINGTONE, RINGTONE}?
    // From current UX spec, there is no chance that an user may enable speaker
    // during alerting, so basically this 'set speaker enable' action can't
    // happen in B2G.
    .then(() => check(PHONE_STATE_RINGTONE, PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))
    .then(() => answer(inCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Hang up all
    .then(() => remoteHangUp(inCall))
    .then(() => check(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // Dial out
    .then(() => dial(outNumber))
    .then(call => { outCall = call; })
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => remoteAnswer(outCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Hang up all
    .then(() => remoteHangUp(outCall))
    .then(() => check(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // Dial out
    .then(() => dial(outNumber))
    .then(call => { outCall = call; })
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => remoteAnswer(outCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Dial out and dial in
    .then(() => remoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    .then(() => answer(inCall))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Conference
    .then(() => addCallsToConference([outCall, inCall]))
    .then(() => check(PHONE_STATE_IN_CALL, PHONE_STATE_IN_CALL))
    // Hang up all
    .then(() => remoteHangUpCalls([outCall, inCall]))
    .then(() => check(PHONE_STATE_NORMAL, PHONE_STATE_NORMAL))

    // End
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
