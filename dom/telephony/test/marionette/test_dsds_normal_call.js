/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

/**
 * The functions are created to provide the string format of the emulator call
 * list results.
 *
 * Usage:
 *   let outInfo = OutCallStrPool("911");
 *   outInfo.ringing == "outbound to 911        : ringing"
 *   outInfo.active  == "outbound to 911        : active"
 */
function CallStrPool(prefix, number) {
  let padding = "           : ";
  let numberInfo = prefix + number + padding.substr(number.length);

  let info = {};
  let states = ["ringing", "incoming", "active", "held"];
  for (let state of states) {
    info[state] = numberInfo + state;
  }

  return info;
}

function OutCallStrPool(number) {
  return CallStrPool("outbound to  ", number);
}

function InCallStrPool(number) {
  return CallStrPool("inbound from ", number);
}

function checkEventCallState(event, call, state) {
  is(call, event.call, "event.call");
  is(call.state, state, "call state");
}

function checkTelephonyActiveAndCalls(active, calls) {
  is(telephony.active, active, "telephony.active");
  is(telephony.calls.length, calls.length, "telephony.calls");
  for (let i = 0; i < calls.length; ++i) {
    is(telephony.calls[i], calls[i]);
  }
}

function checkEmulatorCallList(expectedCallList) {
  let deferred = Promise.defer();

  emulator.run("gsm list", function(result) {
    log("Call list is now: " + result);
    for (let i = 0; i < expectedCallList.length; ++i) {
      is(result[i], expectedCallList[i], "emulator calllist");
    }
    is(result[expectedCallList.length], "OK", "emulator calllist");
    deferred.resolve();
  });

  return deferred.promise;
}

// Promise.
function checkAll(active, calls, callList) {
  checkTelephonyActiveAndCalls(active, calls);
  return checkEmulatorCallList(callList);
}

function dial(number, serviceId) {
  serviceId = typeof serviceId !== "undefined" ? serviceId : 0;
  log("Make an outgoing call: " + number + ", serviceId: " + serviceId);

  let deferred = Promise.defer();
  let call = telephony.dial(number, serviceId);

  ok(call);
  is(call.number, number);
  is(call.state, "dialing");

  call.onalerting = function onalerting(event) {
    call.onalerting = null;
    log("Received 'onalerting' call event.");
    is(call.serviceId, serviceId);
    checkEventCallState(event, call, "alerting");
    deferred.resolve(call);
  };

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

function muxModem(id) {
  let deferred = Promise.defer();

  emulator.run("mux modem " + id, function() {
    deferred.resolve();
  });

  return deferred.promise;
}

function testOutgoingCallForServiceId(number, serviceId) {
  let outCall;
  let outInfo = OutCallStrPool(number);

  return Promise.resolve()
    .then(() => dial(number, serviceId))
    .then(call => {
      outCall = call;
      is(outCall.serviceId, serviceId);
    })
    .then(() => checkAll(outCall, [outCall], [outInfo.ringing]))
    .then(() => remoteAnswer(outCall))
    .then(() => checkAll(outCall, [outCall], [outInfo.active]))
    .then(() => remoteHangUp(outCall))
    .then(() => checkAll(null, [], []));
}

function testIncomingCallForServiceId(number, serviceId) {
  let inCall;
  let inInfo = InCallStrPool(number);

  return Promise.resolve()
    .then(() => remoteDial(number))
    .then(call => {
      inCall = call;
      is(inCall.serviceId, serviceId);
    })
    .then(() => checkAll(null, [inCall], [inInfo.incoming]))
    .then(() => answer(inCall))
    .then(() => checkAll(inCall, [inCall], [inInfo.active]))
    .then(() => remoteHangUp(inCall))
    .then(() => checkAll(null, [], []));
}

function testOutgoingCall() {
  log("= testOutgoingCall =");

  return Promise.resolve()
    .then(() => muxModem(0))
    .then(() => testOutgoingCallForServiceId("0912345000", 0))
    .then(() => muxModem(1))
    .then(() => testOutgoingCallForServiceId("0912345001", 1))
    .then(() => muxModem(0));
}

function testIncomingCall() {
  log("= testIncomingCall =");

  return Promise.resolve()
    .then(() => muxModem(0))
    .then(() => testIncomingCallForServiceId("0912345000", 0))
    .then(() => muxModem(1))
    .then(() => testIncomingCallForServiceId("0912345001", 1))
    .then(() => muxModem(0));
}

startDSDSTest(function() {
  testOutgoingCall()
    .then(testIncomingCall)
    .then(null, () => {
      ok(false, "promise rejects during test.");
    })
    .then(finish);
});
