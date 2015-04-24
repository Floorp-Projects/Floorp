/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

var timeoutCallback = null;
var timeoutDelayMs = 0;
const TIMER_ID = 1234;
const TIMEOUT_VALUE = 300000;  // 5 mins.

// No window in xpcshell-test. Create our own timer mechanism.

function setTimeout(callback, timeoutMs) {
  timeoutCallback = callback;
  timeoutDelayMs = timeoutMs;
  equal(timeoutMs, TIMEOUT_VALUE);
  return TIMER_ID;
}

function clearTimeout(timeoutId) {
  equal(timeoutId, TIMER_ID);
  timeoutCallback = null;
}

function fireTimeout() {
  notEqual(timeoutCallback, null);
  if (timeoutCallback) {
    timeoutCallback();
    timeoutCallback = null;
  }
}

add_test(function test_enter_emergencyCbMode() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  // Do it twice. Should always send the event.
  for (let i = 0; i < 2; ++i) {
    context.RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE]();
    let postedMessage = workerHelper.postedMessage;

    // Should store the mode.
    equal(context.RIL._isInEmergencyCbMode, true);

    // Should notify change.
    equal(postedMessage.rilMessageType, "emergencyCbModeChange");
    equal(postedMessage.active, true);
    equal(postedMessage.timeoutMs, TIMEOUT_VALUE);

    // Should start timer.
    equal(context.RIL._exitEmergencyCbModeTimeoutID, TIMER_ID);
  }

  run_next_test();
});

add_test(function test_exit_emergencyCbMode() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE]();
  context.RIL[UNSOLICITED_EXIT_EMERGENCY_CALLBACK_MODE]();
  let postedMessage = workerHelper.postedMessage;

  // Should store the mode.
  equal(context.RIL._isInEmergencyCbMode, false);

  // Should notify change.
  equal(postedMessage.rilMessageType, "emergencyCbModeChange");
  equal(postedMessage.active, false);

  // Should clear timer.
  equal(context.RIL._exitEmergencyCbModeTimeoutID, null);

  run_next_test();
});

add_test(function test_request_exit_emergencyCbMode_when_timeout() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE]();
  equal(context.RIL._isInEmergencyCbMode, true);
  equal(context.RIL._exitEmergencyCbModeTimeoutID, TIMER_ID);

  let parcelTypes = [];
  context.Buf.newParcel = function(type, options) {
    parcelTypes.push(type);
  };

  // Timeout.
  fireTimeout();

  // Should clear timeout event.
  equal(context.RIL._exitEmergencyCbModeTimeoutID, null);

  // Check indeed sent out REQUEST_EXIT_EMERGENCY_CALLBACK_MODE.
  notEqual(parcelTypes.indexOf(REQUEST_EXIT_EMERGENCY_CALLBACK_MODE), -1);

  run_next_test();
});

add_test(function test_request_exit_emergencyCbMode_when_dial() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE]();
  equal(context.RIL._isInEmergencyCbMode, true);
  equal(context.RIL._exitEmergencyCbModeTimeoutID, TIMER_ID);

  let parcelTypes = [];
  context.Buf.newParcel = function(type, options) {
    parcelTypes.push(type);
  };

  // Dial non-emergency call.
  context.RIL.dial({number: "0912345678",
                    isEmergency: false,
                    isDialEmergency: false});

  // Should clear timeout event.
  equal(context.RIL._exitEmergencyCbModeTimeoutID, null);

  // Check indeed sent out REQUEST_EXIT_EMERGENCY_CALLBACK_MODE.
  notEqual(parcelTypes.indexOf(REQUEST_EXIT_EMERGENCY_CALLBACK_MODE), -1);

  run_next_test();
});

add_test(function test_request_exit_emergencyCbMode_explicitly() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL[UNSOLICITED_ENTER_EMERGENCY_CALLBACK_MODE]();
  equal(context.RIL._isInEmergencyCbMode, true);
  equal(context.RIL._exitEmergencyCbModeTimeoutID, TIMER_ID);

  let parcelTypes = [];
  context.Buf.newParcel = function(type, options) {
    parcelTypes.push(type);
  };

  context.RIL.handleChromeMessage({rilMessageType: "exitEmergencyCbMode"});
  context.RIL[REQUEST_EXIT_EMERGENCY_CALLBACK_MODE](1, {
    rilMessageType: "exitEmergencyCbMode"
  });
  let postedMessage = workerHelper.postedMessage;

  // Should clear timeout event.
  equal(context.RIL._exitEmergencyCbModeTimeoutID, null);

  // Check indeed sent out REQUEST_EXIT_EMERGENCY_CALLBACK_MODE.
  notEqual(parcelTypes.indexOf(REQUEST_EXIT_EMERGENCY_CALLBACK_MODE), -1);

  // Send back the response.
  equal(postedMessage.rilMessageType, "exitEmergencyCbMode");

  run_next_test();
});
