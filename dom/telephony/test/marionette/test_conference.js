/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let conference;

/**
 * The functions are created to provide the string format of the emulator call
 * list results.
 *
 * Usage:
 *   let outInfo = OutCallStrPool('911');
 *   outInfo.ringing == "outbound to 911        : ringing"
 *   outInfo.active  == "outbound to 911        : active"
 */
function CallStrPool(prefix, number) {
  let padding = "           : ";
  let numberInfo = prefix + number + padding.substr(number.length);

  let info = {};
  let states = ['ringing', 'incoming', 'active', 'held'];
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

function checkState(active, calls, conferenceState, conferenceCalls) {
  checkTelephonyActiveAndCalls(active, calls);
  checkConferenceStateAndCalls(conferenceState, conferenceCalls);
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
function checkAll(active, calls, conferenceState, conferenceCalls, callList) {
  checkState(active, calls, conferenceState, conferenceCalls);
  return checkEmulatorCallList(callList);
}

// Make sure there's no pending event before we jump to the next case.
function receivedPending(received, pending, nextAction) {
  let index = pending.indexOf(received);
  if (index != -1) {
    pending.splice(index, 1);
  }
  if (pending.length === 0) {
    nextAction();
  }
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

// Answering an incoming call could trigger conference state change.
function answer(call, conferenceStateChangeCallback) {
  log("Answering the incoming call.");

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve(call);
  };

  let pending = ["call.onconnected"];
  let receive = function(name) {
    receivedPending(name, pending, done);
  };

  // When there's already a connected conference call, answering a new incoming
  // call triggers conference state change. We should wait for
  // |conference.onstatechange| before checking the state of the conference call.
  if (conference.state === "connected") {
    pending.push("conference.onstatechange");
    check_onstatechange(conference, "conference", "held", function() {
      if (typeof conferenceStateChangeCallback === "function") {
        conferenceStateChangeCallback();
      }
      receive("conference.onstatechange");
    });
  }

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
    receive("call.onconnected");
  };
  call.answer();

  return deferred.promise;
}

function remoteDial(number) {
  log("Simulating an incoming call.");

  let deferred = Promise.defer();

  telephony.onincoming = function onincoming(event) {
    log("Received 'incoming' call event.");
    telephony.onincoming = null;

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

// container might be telephony or conference.
function check_oncallschanged(container, containerName, expectedCalls,
                              callback) {
  container.oncallschanged = function(event) {
    log("Received 'callschanged' event for the " + containerName);
    if (event.call) {
      let index = expectedCalls.indexOf(event.call);
      ok(index != -1);
      expectedCalls.splice(index, 1);

      if (expectedCalls.length === 0) {
        container.oncallschanged = null;
        callback();
      }
    }
  };
}

function check_ongroupchange(call, callName, group, callback) {
  call.ongroupchange = function(event) {
    log("Received 'groupchange' event for the " + callName);
    call.ongroupchange = null;

    is(call.group, group);
    callback();
  };
}

function check_onstatechange(call, callName, state, callback) {
  call.onstatechange = function(event) {
    log("Received 'statechange' event for the " + callName);
    call.onstatechange = null;

    is(call.state, state);
    callback();
  };
}

function StateEventChecker(state, previousEvent) {
  let event = 'on' + state;

  return function(call, callName, callback) {
    call[event] = function() {
      log("Received '" + state + "' event for the " + callName);
      call[event] = null;

      if (previousEvent) {
        // We always clear the event handler when the event is received.
        // Therefore, if the corresponding handler is not existed, the expected
        // previous event has been already received.
        ok(!call[previousEvent]);
      }
      is(call.state, state);
      callback();
    };
  };
}

let check_onholding    = StateEventChecker('holding', null);
let check_onheld       = StateEventChecker('held', 'onholding');
let check_onresuming   = StateEventChecker('resuming', null);
let check_onconnected  = StateEventChecker('connected', 'onresuming');

// The length of callsToAdd should be 1 or 2.
function addCallsToConference(callsToAdd, connectedCallback) {
  log("Add " + callsToAdd.length + " calls into conference.");

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["conference.oncallschanged", "conference.onconnected"];
  let receive = function(name) {
    receivedPending(name, pending, done);
  };

  for (let call of callsToAdd) {
    let callName = "callToAdd (" + call.number + ')';

    let ongroupchange = callName + ".ongroupchange";
    pending.push(ongroupchange);
    check_ongroupchange(call, callName, conference,
                        receive.bind(null, ongroupchange));

    let onstatechange = callName + ".onstatechange";
    pending.push(onstatechange);
    check_onstatechange(call, callName, 'connected',
                        receive.bind(null, onstatechange));
  }

  check_oncallschanged(conference, 'conference', callsToAdd,
                       receive.bind(null, "conference.oncallschanged"));

  check_onconnected(conference, "conference", function() {
    ok(!conference.oncallschanged);
    if (typeof connectedCallback === 'function') {
      connectedCallback();
    }
    receive("conference.onconnected");
  });

  // Cannot use apply() through webidl, so just separate the cases to handle.
  if (callsToAdd.length == 2) {
    conference.add(callsToAdd[0], callsToAdd[1]);
  } else {
    conference.add(callsToAdd[0]);
  }

  return deferred.promise;
}

function holdConference(calls, heldCallback) {
  log("Holding the conference call.");

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["conference.onholding", "conference.onheld"];
  let receive = function(name) {
    receivedPending(name, pending, done);
  };

  for (let call of calls) {
    let callName = "call (" + call.number + ')';

    let onholding = callName + ".onholding";
    pending.push(onholding);
    check_onholding(call, callName, receive.bind(null, onholding));

    let onheld = callName + ".onheld";
    pending.push(onheld);
    check_onheld(call, callName, receive.bind(null, onheld));
  }

  check_onholding(conference, "conference",
                  receive.bind(null, "conference.onholding"));

  check_onheld(conference, "conference", function() {
    if (typeof heldCallback === 'function') {
      heldCallback();
    }
    receive("conference.onheld");
  });

  conference.hold();

  return deferred.promise;
}

function resumeConference(calls, connectedCallback) {
  log("Resuming the held conference call.");

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["conference.onresuming", "conference.onconnected"];
  let receive = function(name) {
    receivedPending(name, pending, done);
  };

  for (let call of calls) {
    let callName = "call (" + call.number + ')';

    let onresuming = callName + ".onresuming";
    pending.push(onresuming);
    check_onresuming(call, callName, receive.bind(null, onresuming));

    let onconnected = callName + ".onconnected";
    pending.push(onconnected);
    check_onconnected(call, callName, receive.bind(null, onconnected));
  }

  check_onresuming(conference, "conference",
                   receive.bind(null, "conference.onresuming"));

  check_onconnected(conference, "conference", function() {
    if (typeof connectedCallback === 'function') {
      connectedCallback();
    }
    receive("conference.onconnected");
  });

  conference.resume();

  return deferred.promise;
}

// The length of autoRemovedCalls should be 0 or 1.
function removeCallInConference(callToRemove, autoRemovedCalls, remainedCalls,
                                statechangeCallback) {
  log("Removing a participant from the conference call.");

  is(conference.state, 'connected');

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["callToRemove.ongroupchange", "telephony.oncallschanged",
                 "conference.oncallschanged", "conference.onstatechange"];
  let receive = function(name) {
    receivedPending(name, pending, done);
  };

  // Remained call in conference will be held.
  for (let call of remainedCalls) {
    let callName = "remainedCall (" + call.number + ')';

    let onstatechange = callName + ".onstatechange";
    pending.push(onstatechange);
    check_onstatechange(call, callName, 'held',
                        receive.bind(null, onstatechange));
  }

  // When a call is removed from conference with 2 calls, another one will be
  // automatically removed from group and be put on hold.
  for (let call of autoRemovedCalls) {
    let callName = "autoRemovedCall (" + call.number + ')';

    let ongroupchange = callName + ".ongroupchange";
    pending.push(ongroupchange);
    check_ongroupchange(call, callName, null,
                        receive.bind(null, ongroupchange));

    let onstatechange = callName + ".onstatechange";
    pending.push(onstatechange);
    check_onstatechange(call, callName, 'held',
                        receive.bind(null, onstatechange));
  }

  check_ongroupchange(callToRemove, "callToRemove", null, function() {
    is(callToRemove.state, 'connected');
    receive("callToRemove.ongroupchange");
  });

  check_oncallschanged(telephony, 'telephony',
                       autoRemovedCalls.concat(callToRemove),
                       receive.bind(null, "telephony.oncallschanged"));

  check_oncallschanged(conference, 'conference',
                       autoRemovedCalls.concat(callToRemove), function() {
    is(conference.calls.length, remainedCalls.length);
    receive("conference.oncallschanged");
  });

  check_onstatechange(conference, 'conference',
                      (remainedCalls.length ? 'held' : ''), function() {
    ok(!conference.oncallschanged);
    if (typeof statechangeCallback === 'function') {
      statechangeCallback();
    }
    receive("conference.onstatechange");
  });

  conference.remove(callToRemove);

  return deferred.promise;
}

// The length of autoRemovedCalls should be 0 or 1.
function hangUpCallInConference(callToHangUp, autoRemovedCalls, remainedCalls,
                                statechangeCallback) {
  log("Release one call in conference.");

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["conference.oncallschanged", "remoteHangUp"];
  let receive = function(name) {
    receivedPending(name, pending, done);
  };

  // When a call is hang up from conference with 2 calls, another one will be
  // automatically removed from group.
  for (let call of autoRemovedCalls) {
    let callName = "autoRemovedCall (" + call.number + ')';

    let ongroupchange = callName + ".ongroupchange";
    pending.push(ongroupchange);
    check_ongroupchange(call, callName, null,
                        receive.bind(null, ongroupchange));
  }

  if (autoRemovedCalls.length) {
    pending.push("telephony.oncallschanged");
    check_oncallschanged(telephony, 'telephony',
                         autoRemovedCalls,
                         receive.bind(null, "telephony.oncallschanged"));
  }

  check_oncallschanged(conference, 'conference',
                       autoRemovedCalls.concat(callToHangUp), function() {
    is(conference.calls.length, remainedCalls.length);
    receive("conference.oncallschanged");
  });

  if (remainedCalls.length === 0) {
    pending.push("conference.onstatechange");
    check_onstatechange(conference, 'conference', '', function() {
      ok(!conference.oncallschanged);
      if (typeof statechangeCallback === 'function') {
        statechangeCallback();
      }
      receive("conference.onstatechange");
    });
  }

  remoteHangUp(callToHangUp)
    .then(receive.bind(null, "remoteHangUp"));

  return deferred.promise;
}

/**
 * Setup a conference with an outgoing call and an incoming call.
 *
 * @return Promise<[outCall, inCall]>
 */
function setupConferenceTwoCalls(outNumber, inNumber) {
  log('Create conference with two calls.');

  let outCall;
  let inCall;
  let outInfo = OutCallStrPool(outNumber);
  let inInfo = InCallStrPool(inNumber);

  return Promise.resolve()
    .then(checkInitialState)
    .then(() => dial(outNumber))
    .then(call => { outCall = call; })
    .then(() => checkAll(outCall, [outCall], '', [], [outInfo.ringing]))
    .then(() => remoteAnswer(outCall))
    .then(() => checkAll(outCall, [outCall], '', [], [outInfo.active]))
    .then(() => remoteDial(inNumber))
    .then(call => { inCall = call; })
    .then(() => checkAll(outCall, [outCall, inCall], '', [],
                         [outInfo.active, inInfo.incoming]))
    .then(() => answer(inCall))
    .then(() => checkAll(inCall, [outCall, inCall], '', [],
                         [outInfo.held, inInfo.active]))
    .then(() => addCallsToConference([outCall, inCall], function() {
      checkState(conference, [], 'connected', [outCall, inCall]);
    }))
    .then(() => checkAll(conference, [], 'connected', [outCall, inCall],
                         [outInfo.active, inInfo.active]))
    .then(() => {
      return [outCall, inCall];
    });
}

/**
 * Setup a conference with an outgoing call and two incoming calls.
 *
 * @return Promise<[outCall, inCall, inCall2]>
 */
function setupConferenceThreeCalls(outNumber, inNumber, inNumber2) {
  log('Create conference with three calls.');

  let outCall;
  let inCall;
  let inCall2;
  let outInfo = OutCallStrPool(outNumber);
  let inInfo = InCallStrPool(inNumber);
  let inInfo2 = InCallStrPool(inNumber2);

  return Promise.resolve()
    .then(() => setupConferenceTwoCalls(outNumber, inNumber))
    .then(calls => {
      outCall = calls[0];
      inCall = calls[1];
    })
    .then(() => remoteDial(inNumber2))
    .then(call => { inCall2 = call; })
    .then(() => checkAll(conference, [inCall2], 'connected', [outCall, inCall],
                         [outInfo.active, inInfo.active, inInfo2.incoming]))
    .then(() => answer(inCall2, function() {
      checkState(inCall2, [inCall2], 'held', [outCall, inCall]);
    }))
    .then(() => checkAll(inCall2, [inCall2], 'held', [outCall, inCall],
                         [outInfo.held, inInfo.held, inInfo2.active]))
    .then(() => addCallsToConference([inCall2], function() {
      checkState(conference, [], 'connected', [outCall, inCall, inCall2]);
    }))
    .then(() => checkAll(conference, [],
                         'connected', [outCall, inCall, inCall2],
                         [outInfo.active, inInfo.active, inInfo2.active]))
    .then(() => {
      return [outCall, inCall, inCall2];
    });
}

function testConferenceTwoCalls() {
  log('= testConferenceTwoCalls =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";

  return Promise.resolve()
    .then(() => setupConferenceTwoCalls(outNumber, inNumber))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => remoteHangUpCalls([outCall, inCall]));
}

function testConferenceHoldAndResume() {
  log('= testConferenceHoldAndResume =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let outInfo = OutCallStrPool(outNumber);
  let inInfo = InCallStrPool(inNumber);

  return Promise.resolve()
    .then(() => setupConferenceTwoCalls(outNumber, inNumber))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => holdConference([outCall, inCall], function() {
      checkState(null, [], 'held', [outCall, inCall]);
    }))
    .then(() => checkAll(null, [], 'held', [outCall, inCall],
                         [outInfo.held, inInfo.held]))
    .then(() => resumeConference([outCall, inCall], function() {
      checkState(conference, [], 'connected', [outCall, inCall]);
    }))
    .then(() => checkAll(conference, [], 'connected', [outCall, inCall],
                         [outInfo.active, inInfo.active]))
    .then(() => remoteHangUpCalls([outCall, inCall]));
}

function testConferenceThreeAndRemoveOne() {
  log('= testConferenceThreeAndRemoveOne =');

  let outCall;
  let inCall;
  let inCall2;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inNumber2 = "5555550202";
  let outInfo = OutCallStrPool(outNumber);
  let inInfo = InCallStrPool(inNumber);
  let inInfo2 = InCallStrPool(inNumber2);

  return Promise.resolve()
    .then(() => setupConferenceThreeCalls(outNumber, inNumber, inNumber2))
    .then(calls => {
      [outCall, inCall, inCall2] = calls;
    })
    .then(() => removeCallInConference(outCall, [], [inCall, inCall2],
                                       function() {
        checkState(outCall, [outCall], 'held', [inCall, inCall2]);
    }))
    .then(() => checkAll(outCall, [outCall], 'held', [inCall, inCall2],
                         [outInfo.active, inInfo.held, inInfo2.held]))
    .then(() => remoteHangUpCalls([outCall, inCall, inCall2]));
}

function testConferenceThreeAndHangupOne() {
  log('= testConferenceThreeAndHangupOne =');

  let outCall;
  let inCall;
  let inCall2;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inNumber2 = "5555550202";
  let inInfo = InCallStrPool(inNumber);
  let inInfo2 = InCallStrPool(inNumber2);

  return Promise.resolve()
    .then(() => setupConferenceThreeCalls(outNumber, inNumber, inNumber2))
    .then(calls => {
      [outCall, inCall, inCall2] = calls;
    })
    .then(() => hangUpCallInConference(outCall, [], [inCall, inCall2]))
    .then(() => checkAll(conference, [], 'connected', [inCall, inCall2],
                         [inInfo.active, inInfo2.active]))
    .then(() => remoteHangUpCalls([inCall, inCall2]));
}

function testConferenceTwoAndRemoveOne() {
  log('= testConferenceTwoAndRemoveOne =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let outInfo = OutCallStrPool(outNumber);
  let inInfo = InCallStrPool(inNumber);

  return Promise.resolve()
    .then(() => setupConferenceTwoCalls(outNumber, inNumber))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => removeCallInConference(outCall, [inCall], [], function() {
      checkState(outCall, [outCall, inCall], '', []);
    }))
    .then(() => checkAll(outCall, [outCall, inCall], '', [],
                         [outInfo.active, inInfo.held]))
    .then(() => remoteHangUpCalls([outCall, inCall]));
}

function testConferenceTwoAndHangupOne() {
  log('= testConferenceTwoAndHangupOne =');

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber  = "5555550201";
  let inInfo = InCallStrPool(inNumber);

  return Promise.resolve()
    .then(() => setupConferenceTwoCalls(outNumber, inNumber))
    .then(calls => {
      [outCall, inCall] = calls;
    })
    .then(() => hangUpCallInConference(outCall, [inCall], [], function() {
      checkState(inCall, [inCall], '', []);
    }))
    .then(() => checkAll(inCall, [inCall], '', [], [inInfo.active]))
    .then(() => remoteHangUpCalls([inCall]));
}

// Start the test
startTest(function() {
  conference = telephony.conferenceGroup;
  ok(conference);

  testConferenceTwoCalls()
    .then(testConferenceHoldAndResume)
    .then(testConferenceThreeAndRemoveOne)
    .then(testConferenceThreeAndHangupOne)
    .then(testConferenceTwoAndRemoveOne)
    .then(testConferenceTwoAndHangupOne)
    .then(null, error => {
      ok(false, 'promise rejects during test.');
    })
    .then(finish);
});
