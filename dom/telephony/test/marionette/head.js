/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Emulate Promise.jsm semantics.
Promise.defer = function() { return new Deferred(); };
function Deferred()  {
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
  Object.freeze(this);
}

let telephony;
let conference;

const kPrefRilDebuggingEnabled = "ril.debugging.enabled";

/**
 * Emulator helper.
 */
let emulator = (function() {
  let pendingCmdCount = 0;
  let originalRunEmulatorCmd = runEmulatorCmd;

  let pendingShellCount = 0;
  let originalRunEmulatorShell = runEmulatorShell;

  // Overwritten it so people could not call this function directly.
  runEmulatorCmd = function() {
    throw "Use emulator.runCmdWithCallback(cmd, callback) instead of runEmulatorCmd";
  };

  // Overwritten it so people could not call this function directly.
  runEmulatorShell = function() {
    throw "Use emulator.runShellCmd(cmd, callback) instead of runEmulatorShell";
  };

  function runCmd(cmd) {
    let deferred = Promise.defer();

    pendingCmdCount++;
    originalRunEmulatorCmd(cmd, function(result) {
      pendingCmdCount--;
      if (result[result.length - 1] === "OK") {
        deferred.resolve(result);
      } else {
        is(result[result.length - 1], "OK", "emulator command result.");
        deferred.reject();
      }
    });

    return deferred.promise;
  }

  function runCmdWithCallback(cmd, callback) {
    runCmd(cmd).then(result => {
      if (callback && typeof callback === "function") {
        callback(result);
      }
    });
  }

  /**
   * @return Promise
   */
  function runShellCmd(aCommands) {
    let deferred = Promise.defer();

    ++pendingShellCount;
    originalRunEmulatorShell(aCommands, function(aResult) {
      --pendingShellCount;
      deferred.resolve(aResult);
    });

    return deferred.promise;
  }

  /**
   * @return Promise
   */
  function waitFinish() {
    let deferred = Promise.defer();

    waitFor(function() {
      deferred.resolve();
    }, function() {
      return pendingCmdCount === 0 && pendingShellCount === 0;
    });

    return deferred.promise;
  }

  return {
    runCmd: runCmd,
    runCmdWithCallback: runCmdWithCallback,
    runShellCmd: runShellCmd,
    waitFinish: waitFinish
  };
}());

/**
 * Telephony related helper functions.
 */
(function() {
  /**
   * @return Promise
   */
  function delay(ms) {
    let deferred = Promise.defer();

    let startTime = Date.now();
    waitFor(function() {
      deferred.resolve();
    },function() {
      let duration = Date.now() - startTime;
      return (duration >= ms);
    });

    return deferred.promise;
  }

  /**
   * @return Promise
   */
  function waitForNoCall() {
    let deferred = Promise.defer();

    waitFor(function() {
      deferred.resolve();
    }, function() {
      return telephony.calls.length === 0;
    });

    return deferred.promise;
  }

  /**
   * @return Promise
   */
  function clearCalls() {
    log("Clear existing calls.");

    // Hang up all calls.
    let hangUpPromises = [];

    for (let call of telephony.calls) {
      log(".. hangUp " + call.id.number);
      hangUpPromises.push(hangUp(call));
    }

    for (let call of conference.calls) {
      log(".. hangUp " + call.id.number);
      hangUpPromises.push(hangUp(call));
    }

    return Promise.all(hangUpPromises)
      .then(() => {
        return emulator.runCmd("gsm clear").then(waitForNoCall);
      })
      .then(waitForNoCall);
  }

  /**
   * Provide a string with format of the emulator call list result.
   *
   * @param prefix
   *        Possible values are "outbound" and "inbound".
   * @param number
   *        Call number.
   * @return A string with format of the emulator call list result.
   */
  function callStrPool(prefix, number) {
    let padding = "           : ";
    let numberInfo = prefix + number + padding.substr(number.length);

    let info = {};
    let states = ['ringing', 'incoming', 'active', 'held'];
    for (let state of states) {
      info[state] = numberInfo + state;
    }

    return info;
  }

  /**
   * Provide a corresponding string of an outgoing call. The string is with
   * format of the emulator call list result.
   *
   * @param number
   *        Number of an outgoing call.
   * @return A string with format of the emulator call list result.
   */
  function outCallStrPool(number) {
    return callStrPool("outbound to  ", number);
  }

  /**
   * Provide a corresponding string of an incoming call. The string is with
   * format of the emulator call list result.
   *
   * @param number
   *        Number of an incoming call.
   * @return A string with format of the emulator call list result.
   */
  function inCallStrPool(number) {
    return callStrPool("inbound from ", number);
  }

  /**
   * Check utility functions.
   */

  function checkInitialState() {
    log("Verify initial state.");
    ok(telephony.calls, 'telephony.call');
    checkTelephonyActiveAndCalls(null, []);
    ok(conference.calls, 'conference.calls');
    checkConferenceStateAndCalls('', []);
  }

  /**
   * Convenient helper to compare a TelephonyCall and a received call event.
   */
  function checkEventCallState(event, call, state) {
    is(call, event.call, "event.call");
    is(call.state, state, "call state");
  }

  /**
   * Convenient helper to compare two call lists. Size should be the same and
   * order is not important.
   */
  function checkCalls(actualCalls, expectedCalls) {
    if (actualCalls.length == expectedCalls.length) {
      let expectedSet = new Set(expectedCalls);
      for (let i = 0; i < actualCalls.length; ++i) {
        ok(expectedSet.has(actualCalls[i]), "should contain the call");
      }
    }
  }

  /**
   * Convenient helper to check mozTelephony.active and mozTelephony.calls.
   */
  function checkTelephonyActiveAndCalls(active, calls) {
    is(telephony.active, active, "telephony.active");
    is(telephony.calls.length, calls.length, "telephony.calls");
    checkCalls(telephony.calls, calls);
  }

  /**
   * Convenient helper to check mozTelephony.conferenceGroup.state and
   * .conferenceGroup.calls.
   */
  function checkConferenceStateAndCalls(state, calls) {
    is(conference.state, state, "conference.state");
    is(conference.calls.length, calls.length, "conference.calls");
    checkCalls(conference.calls, calls);
  }

  /**
   * Convenient helper to handle *.oncallschanged event.
   *
   * @param container
   *        Representation of "mozTelephony" or "mozTelephony.conferenceGroup."
   * @param containerName
   *        Name of container. Could be an arbitrary string, used for debug
   *        messages only.
   * @param expectedCalls
   *        An array of calls.
   * @param callback
   *        A callback function.
   */
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

  /**
   * Convenient helper to handle *.ongroupchange event.
   *
   * @param call
   *        A TelephonyCall object.
   * @param callName
   *        Name of a call. Could be an arbitrary string, used for debug messages
   *        only.
   * @param group
   *        Representation of mozTelephony.conferenceGroup.
   * @param callback
   *        A callback function.
   */
  function check_ongroupchange(call, callName, group, callback) {
    call.ongroupchange = function(event) {
      log("Received 'groupchange' event for the " + callName);
      call.ongroupchange = null;

      is(call.group, group);
      callback();
    };
  }

  /**
   * Convenient helper to handle *.onstatechange event.
   *
   * @param container
   *        Representation of a TelephonyCall or mozTelephony.conferenceGroup.
   * @param containerName
   *        Name of container. Could be an arbitrary string, used for debug messages
   *        only.
   * @param state
   *        A string.
   * @param callback
   *        A callback function.
   */
  function check_onstatechange(container, containerName, state, callback) {
    container.onstatechange = function(event) {
      log("Received 'statechange' event for the " + containerName);
      container.onstatechange = null;

      is(container.state, state);
      callback();
    };
  }

  /**
   * Convenient helper to check the sequence of call state and event handlers.
   *
   * @param state
   *        A string of the expected call state.
   * @param previousEvent
   *        A string of the event that should come before the expected state.
   */
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

  /**
   * Convenient helper to check the expected call number and name.
   *
   * @param number
   *        A string sent to modem.
   * @param numberPresentation
   *        An unsigned short integer sent to modem.
   * @param name
   *        A string sent to modem.
   * @param namePresentation
   *        An unsigned short integer sent to modem.
   * @param receivedNumber
   *        A string exposed by Telephony API.
   * @param receivedName
   *        A string exposed by Telephony API.
   */
  function checkCallId(number, numberPresentation, name, namePresentation,
                       receivedNumber, receivedName) {
    let expectedNum = !numberPresentation ? number : "";
    is(receivedNumber, expectedNum, "check number per numberPresentation");

    let expectedName;
    if (numberPresentation) {
      expectedName = "";
    } else if (!namePresentation) {
      expectedName = name ? name : "";
    } else {
      expectedName = "";
    }
    is(receivedName, expectedName, "check name per number/namePresentation");
  }

  /**
   * Convenient helper to check the call list existing in the emulator.
   *
   * @param expectedCallList
   *        An array of call info with the format of "callStrPool()[state]".
   * @return A deferred promise.
   */
  function checkEmulatorCallList(expectedCallList) {
    return emulator.runCmd("gsm list").then(result => {
      log("Call list is now: " + result);
      for (let i = 0; i < expectedCallList.length; ++i) {
        is(result[i], expectedCallList[i], "emulator calllist");
      }
    });
  }

  /**
   * Super convenient helper to check calls and state of mozTelephony and
   * mozTelephony.conferenceGroup.
   *
   * @param active
   *        A TelephonyCall object. Should be the expected active call.
   * @param calls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.calls.
   * @param conferenceState
   *        A string. Should be the expected conference state.
   * @param conferenceCalls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.conferenceGroup.calls.
   */
  function checkState(active, calls, conferenceState, conferenceCalls) {
    checkTelephonyActiveAndCalls(active, calls);
    checkConferenceStateAndCalls(conferenceState, conferenceCalls);
  }

  /**
   * Super convenient helper to check calls and state of mozTelephony and
   * mozTelephony.conferenceGroup as well as the calls existing in the emulator.
   *
   * @param active
   *        A TelephonyCall object. Should be the expected active call.
   * @param calls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.calls.
   * @param conferenceState
   *        A string. Should be the expected conference state.
   * @param conferenceCalls
   *        An array of TelephonyCall objects. Should be the expected list of
   *        mozTelephony.conferenceGroup.calls.
   * @param callList
   *        An array of call info with the format of "callStrPool()[state]".
   * @return A deferred promise.
   */
  function checkAll(active, calls, conferenceState, conferenceCalls, callList) {
    checkState(active, calls, conferenceState, conferenceCalls);
    return checkEmulatorCallList(callList);
  }

  /**
   * Request utility functions.
   */

  /**
   * Make sure there's no pending event before we jump to the next action.
   *
   * @param received
   *        A string of the received event.
   * @param pending
   *        An array of the pending events.
   * @param nextAction
   *        A callback function that is called when there's no pending event.
   */
  function receivedPending(received, pending, nextAction) {
    let index = pending.indexOf(received);
    if (index != -1) {
      pending.splice(index, 1);
    }
    if (pending.length === 0) {
      nextAction();
    }
  }

  /**
   * Make an outgoing call.
   *
   * @param number
   *        A string.
   * @param serviceId [optional]
   *        Identification of a service. 0 is set as default.
   * @return A deferred promise.
   */
  function dial(number, serviceId) {
    serviceId = typeof serviceId !== "undefined" ? serviceId : 0;
    log("Make an outgoing call: " + number + ", serviceId: " + serviceId);

    let deferred = Promise.defer();

    telephony.dial(number, serviceId).then(call => {
      ok(call);
      is(call.id.number, number);
      is(call.state, "dialing");
      is(call.serviceId, serviceId);

      call.onalerting = function onalerting(event) {
        call.onalerting = null;
        log("Received 'onalerting' call event.");
        checkEventCallState(event, call, "alerting");
        deferred.resolve(call);
      };
    }, cause => {
      deferred.reject(cause);
    });

    return deferred.promise;
  }

  /**
   * Make an outgoing emergency call.
   *
   * @param number
   *        A string.
   * @return A deferred promise.
   */
  function dialEmergency(number) {
    log("Make an outgoing emergency call: " + number);

    let deferred = Promise.defer();

    telephony.dialEmergency(number).then(call => {
      ok(call);
      is(call.id.number, number);
      is(call.state, "dialing");

      call.onalerting = function onalerting(event) {
        call.onalerting = null;
        log("Received 'onalerting' call event.");
        checkEventCallState(event, call, "alerting");
        deferred.resolve(call);
      };
    }, cause => {
      deferred.reject(cause);
    });

    return deferred.promise;
  }

  /**
   * Answer an incoming call.
   *
   * @param call
   *        An incoming TelephonyCall object.
   * @param conferenceStateChangeCallback [optional]
   *        A callback function which is called if answering an incoming call
   *        triggers conference state change.
   * @return A deferred promise.
   */
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

  /**
   * Hold a call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return A deferred promise.
   */
  function hold(call) {
    log("Putting the call on hold.");

    let deferred = Promise.defer();

    let gotHolding = false;
    call.onholding = function onholding(event) {
      log("Received 'holding' call event");
      call.onholding = null;
      checkEventCallState(event, call, "holding");
      gotHolding = true;
    };

    call.onheld = function onheld(event) {
      log("Received 'held' call event");
      call.onheld = null;
      checkEventCallState(event, call, "held");
      ok(gotHolding);
      deferred.resolve(call);
    };
    call.hold();

    return deferred.promise;
  }

  /**
   * Resume a call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return A deferred promise.
   */
  function resume(call) {
    log("Resuming the held call.");

    let deferred = Promise.defer();

    let gotResuming = false;
    call.onresuming = function onresuming(event) {
      log("Received 'resuming' call event");
      call.onresuming = null;
      checkEventCallState(event, call, "resuming");
      gotResuming = true;
    };

    call.onconnected = function onconnected(event) {
      log("Received 'connected' call event");
      call.onconnected = null;
      checkEventCallState(event, call, "connected");
      ok(gotResuming);
      deferred.resolve(call);
    };
    call.resume();

    return deferred.promise;
  }

  /**
   * Locally hang up a call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return A deferred promise.
   */
  function hangUp(call) {
    let deferred = Promise.defer();

    call.ondisconnected = function(event) {
      log("Received 'disconnected' call event");
      call.ondisconnected = null;
      checkEventCallState(event, call, "disconnected");
      deferred.resolve(call);
    };
    call.hangUp();

    return deferred.promise;
  }

  /**
   * Simulate an incoming call.
   *
   * @param number
   *        A string.
   * @param numberPresentation [optional]
   *        An unsigned short integer.
   * @param name [optional]
   *        A string.
   * @param namePresentation [optional]
   *        An unsigned short integer.
   * @return A deferred promise.
   */
  function remoteDial(number, numberPresentation, name, namePresentation) {
    log("Simulating an incoming call.");

    let deferred = Promise.defer();

    telephony.onincoming = function onincoming(event) {
      log("Received 'incoming' call event.");
      telephony.onincoming = null;

      let call = event.call;

      ok(call);
      is(call.state, "incoming");
      checkCallId(number, numberPresentation, name, namePresentation,
                  call.id.number, call.id.name);
      deferred.resolve(call);
    };

    numberPresentation = numberPresentation || "";
    name = name || "";
    namePresentation = namePresentation || "";
    emulator.runCmd("gsm call " + number + "," + numberPresentation + "," + name +
                 "," + namePresentation);
    return deferred.promise;
  }

  /**
   * Remote party answers the call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return A deferred promise.
   */
  function remoteAnswer(call) {
    log("Remote answering the call.");

    let deferred = Promise.defer();

    call.onconnected = function onconnected(event) {
      log("Received 'connected' call event.");
      call.onconnected = null;
      checkEventCallState(event, call, "connected");
      deferred.resolve(call);
    };
    emulator.runCmd("gsm accept " + call.id.number);

    return deferred.promise;
  }

  /**
   * Remote party hangs up the call.
   *
   * @param call
   *        A TelephonyCall object.
   * @return A deferred promise.
   */
  function remoteHangUp(call) {
    log("Remote hanging up the call.");

    let deferred = Promise.defer();

    call.ondisconnected = function ondisconnected(event) {
      log("Received 'disconnected' call event.");
      call.ondisconnected = null;
      checkEventCallState(event, call, "disconnected");
      deferred.resolve(call);
    };
    emulator.runCmd("gsm cancel " + call.id.number);

    return deferred.promise;
  }

  /**
   * Remote party hangs up all the calls.
   *
   * @param calls
   *        An array of TelephonyCall objects.
   * @return A deferred promise.
   */
  function remoteHangUpCalls(calls) {
    let promise = Promise.resolve();

    for (let call of calls) {
      promise = promise.then(remoteHangUp.bind(null, call));
    }

    return promise;
  }

  /**
   * Add calls to conference.
   *
   * @param callsToAdd
   *        An array of TelephonyCall objects to be added into conference. The
   *        length of the array should be 1 or 2.
   * @param connectedCallback [optional]
   *        A callback function which is called when conference state becomes
   *        connected.
   * @param twice [optional]
   *        To send conference request twice. It is only used for special test.
   * @return A deferred promise.
   */
  function addCallsToConference(callsToAdd, connectedCallback, twice) {
    log("Add " + callsToAdd.length + " calls into conference.");

    let deferred = Promise.defer();
    let done = function() {
      deferred.resolve();
    };

    let pending = ["conference.oncallschanged", "conference.onconnected"];
    let receive = function(name) {
      receivedPending(name, pending, done);
    };

    let check_onconnected  = StateEventChecker('connected', 'onresuming');

    for (let call of callsToAdd) {
      let callName = "callToAdd (" + call.id.number + ')';

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
    let requestCount = twice ? 2 : 1;
    for (let i = 0; i < requestCount; ++i) {
      if (callsToAdd.length == 2) {
        conference.add(callsToAdd[0], callsToAdd[1]);
      } else {
        conference.add(callsToAdd[0]);
      }
    }

    return deferred.promise;
  }

  /**
   * Hold the conference.
   *
   * @param calls
   *        An array of TelephonyCall objects existing in conference.
   * @param heldCallback [optional]
   *        A callback function which is called when conference state becomes
   *        held.
   * @return A deferred promise.
   */
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

    let check_onholding = StateEventChecker('holding', null);
    let check_onheld = StateEventChecker('held', 'onholding');

    for (let call of calls) {
      let callName = "call (" + call.id.number + ')';

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

  /**
   * Resume the conference.
   *
   * @param calls
   *        An array of TelephonyCall objects existing in conference.
   * @param connectedCallback [optional]
   *        A callback function which is called when conference state becomes
   *        connected.
   * @return A deferred promise.
   */
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

    let check_onresuming   = StateEventChecker('resuming', null);
    let check_onconnected  = StateEventChecker('connected', 'onresuming');

    for (let call of calls) {
      let callName = "call (" + call.id.number + ')';

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

  /**
   * Remove a call out of conference.
   *
   * @param callToRemove
   *        A TelephonyCall object existing in conference.
   * @param autoRemovedCalls
   *        An array of TelephonyCall objects which is going to be automatically
   *        removed. The length of the array should be 0 or 1.
   * @param remainedCalls
   *        An array of TelephonyCall objects which remain in conference.
   * @param stateChangeCallback [optional]
   *        A callback function which is called when conference state changes.
   * @return A deferred promise.
   */
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
      let callName = "remainedCall (" + call.id.number + ')';

      let onstatechange = callName + ".onstatechange";
      pending.push(onstatechange);
      check_onstatechange(call, callName, 'held',
                          receive.bind(null, onstatechange));
    }

    // When a call is removed from conference with 2 calls, another one will be
    // automatically removed from group and be put on hold.
    for (let call of autoRemovedCalls) {
      let callName = "autoRemovedCall (" + call.id.number + ')';

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

  /**
   * Hangup a call in conference.
   *
   * @param callToHangUp
   *        A TelephonyCall object existing in conference.
   * @param autoRemovedCalls
   *        An array of TelephonyCall objects which is going to be automatically
   *        removed. The length of the array should be 0 or 1.
   * @param remainedCalls
   *        An array of TelephonyCall objects which remain in conference.
   * @param stateChangeCallback [optional]
   *        A callback function which is called when conference state changes.
   * @return A deferred promise.
   */
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
      let callName = "autoRemovedCall (" + call.id.number + ')';

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
   * Hangup conference.
   *
   * @return A deferred promise.
   */
  function hangUpConference() {
    log("Hangup conference.");

    let deferred = Promise.defer();
    let done = function() {
      deferred.resolve();
    };

    let pending = ["conference.hangUp", "conference.onstatechange"];
    let receive = function(name) {
      receivedPending(name, pending, done);
    };

    for (let call of conference.calls) {
      let callName = "Call (" + call.id.number + ')';

      let onstatechange = callName + ".onstatechange";
      pending.push(onstatechange);
      check_onstatechange(call, callName, 'disconnected',
                          receive.bind(null, onstatechange));
    }

    check_onstatechange(conference, 'conference', '', function() {
      receive("conference.onstatechange");
    });

    conference.hangUp().then(() => {
      receive("conference.hangUp");
    });

    return deferred.promise;
  }

  /**
   * Create a conference with an outgoing call and an incoming call.
   *
   * @param outNumber
   *        Number of an outgoing call.
   * @param inNumber
   *        Number of an incoming call.
   * @return Promise<[outCall, inCall]>
   */
  function createConferenceWithTwoCalls(outNumber, inNumber) {
    let outCall;
    let inCall;
    let outInfo = outCallStrPool(outNumber);
    let inInfo = inCallStrPool(inNumber);

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
   * Create a new incoming call and add it into the conference.
   *
   * @param inNumber
   *        Number of an incoming call.
   * @param conferenceCalls
   *        Calls already in conference.
   * @return Promise<[calls in the conference]>
   */
  function createCallAndAddToConference(inNumber, conferenceCalls) {
    // Create an info array. allInfo = [info1, info2, ...].
    let allInfo = conferenceCalls.map(function(call, i) {
      return (i === 0) ? outCallStrPool(call.id.number)
                       : inCallStrPool(call.id.number);
    });

    // Define state property of the info array.
    // Ex: allInfo.active = [info1.active, info2.active, ...].
    function addInfoState(allInfo, state) {
      Object.defineProperty(allInfo, state, {
        get: function() {
          return allInfo.map(function(info) { return info[state]; });
        }
      });
    }

    for (let state of ['ringing', 'incoming', 'active', 'held']) {
      addInfoState(allInfo, state);
    }

    let newCall;
    let newInfo = inCallStrPool(inNumber);

    return remoteDial(inNumber)
      .then(call => { newCall = call; })
      .then(() => checkAll(conference, [newCall], 'connected', conferenceCalls,
                           allInfo.active.concat(newInfo.incoming)))
      .then(() => answer(newCall, function() {
        checkState(newCall, [newCall], 'held', conferenceCalls);
      }))
      .then(() => checkAll(newCall, [newCall], 'held', conferenceCalls,
                           allInfo.held.concat(newInfo.active)))
      .then(() => {
        // We are going to add the new call into the conference.
        conferenceCalls.push(newCall);
        allInfo.push(newInfo);
      })
      .then(() => addCallsToConference([newCall], function() {
        checkState(conference, [], 'connected', conferenceCalls);
      }))
      .then(() => checkAll(conference, [], 'connected', conferenceCalls,
                           allInfo.active))
      .then(() => {
        return conferenceCalls;
      });
  }

  /**
   * Setup a conference with an outgoing call and N incoming calls.
   *
   * @param callNumbers
   *        Array of numbers, the first number is for outgoing call and the
   *        remaining numbers are for incoming calls.
   * @return Promise<[calls in the conference]>
   */
  function setupConference(callNumbers) {
    log("Create a conference with " + callNumbers.length + " calls.");

    let promise = createConferenceWithTwoCalls(callNumbers[0], callNumbers[1]);

    callNumbers.shift();
    callNumbers.shift();
    for (let number of callNumbers) {
      promise = promise.then(createCallAndAddToConference.bind(null, number));
    }

    return promise;
  }

  /**
   * Public members.
   */

  this.gDelay = delay;
  this.gCheckInitialState = checkInitialState;
  this.gClearCalls = clearCalls;
  this.gOutCallStrPool = outCallStrPool;
  this.gInCallStrPool = inCallStrPool;
  this.gCheckState = checkState;
  this.gCheckAll = checkAll;
  this.gDial = dial;
  this.gDialEmergency = dialEmergency;
  this.gAnswer = answer;
  this.gHangUp = hangUp;
  this.gHold = hold;
  this.gResume = resume;
  this.gRemoteDial = remoteDial;
  this.gRemoteAnswer = remoteAnswer;
  this.gRemoteHangUp = remoteHangUp;
  this.gRemoteHangUpCalls = remoteHangUpCalls;
  this.gAddCallsToConference = addCallsToConference;
  this.gHoldConference = holdConference;
  this.gResumeConference = resumeConference;
  this.gRemoveCallInConference = removeCallInConference;
  this.gHangUpCallInConference = hangUpCallInConference;
  this.gHangUpConference = hangUpConference;
  this.gSetupConference = setupConference;
  this.gReceivedPending = receivedPending;
}());

function _startTest(permissions, test) {
  function permissionSetUp() {
    SpecialPowers.setBoolPref("dom.mozSettings.enabled", true);
    for (let per of permissions) {
      SpecialPowers.addPermission(per, true, document);
    }
  }

  function permissionTearDown() {
    SpecialPowers.clearUserPref("dom.mozSettings.enabled");
    for (let per of permissions) {
      SpecialPowers.removePermission(per, document);
    }
  }

  let debugPref;

  function setUp() {
    log("== Test SetUp ==");

    // Turn on debugging pref.
    debugPref = SpecialPowers.getBoolPref(kPrefRilDebuggingEnabled);
    SpecialPowers.setBoolPref(kPrefRilDebuggingEnabled, true);
    log("Set debugging pref: " + debugPref + " => true");

    permissionSetUp();

    // Make sure that we get the telephony after adding permission.
    telephony = window.navigator.mozTelephony;
    ok(telephony);
    conference = telephony.conferenceGroup;
    ok(conference);
    return gClearCalls().then(gCheckInitialState);
  }

  // Extend finish() with tear down.
  finish = (function() {
    let originalFinish = finish;

    function tearDown() {
      log("== Test TearDown ==");
      emulator.waitFinish()
        .then(() => {
          permissionTearDown();

          // Restore debugging pref.
          SpecialPowers.setBoolPref(kPrefRilDebuggingEnabled, debugPref);
          log("Set debugging pref: true => " + debugPref);
        })
        .then(function() {
          originalFinish.apply(this, arguments);
        });
    }

    return tearDown.bind(this);
  }());

  function mainTest() {
    setUp()
      .then(function onSuccess() {
        log("== Test Start ==");
        test();
      }, function onError(error) {
        SpecialPowers.Cu.reportError(error);
        ok(false, "SetUp error");
      });
  }

  mainTest();
}

function startTest(test) {
  _startTest(["telephony"], test);
}

function startTestWithPermissions(permissions, test) {
  _startTest(permissions.concat("telephony"), test);
}

function startDSDSTest(test) {
  let numRIL;
  try {
    numRIL = SpecialPowers.getIntPref("ril.numRadioInterfaces");
  } catch (ex) {
    numRIL = 1;  // Pref not set.
  }

  if (numRIL > 1) {
    startTest(test);
  } else {
    log("Not a DSDS environment. Test is skipped.");
    ok(true);  // We should run at least one test.
    finish();
  }
}

function sendMMI(aMmi) {
  let deferred = Promise.defer();

  telephony.dial(aMmi)
    .then(result => {
      deferred.resolve(result);
    }, cause => {
      deferred.reject(cause);
    });

  return deferred.promise;
}
