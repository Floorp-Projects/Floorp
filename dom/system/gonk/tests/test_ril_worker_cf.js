/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function toaFromString(number) {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });
  let context = worker.ContextPool._contexts[0];
  return context.RIL._toaFromString(number);
}

add_test(function test_toaFromString_empty() {
  let retval = toaFromString("");

  equal(retval, TOA_UNKNOWN);

  run_next_test();
});

add_test(function test_toaFromString_undefined() {
  let retval = toaFromString();

  equal(retval, TOA_UNKNOWN);

  run_next_test();
});

add_test(function test_toaFromString_unknown() {
  let retval = toaFromString("666222333");

  equal(retval, TOA_UNKNOWN);

  run_next_test();
});

add_test(function test_toaFromString_international() {
  let retval = toaFromString("+34666222333");

  equal(retval, TOA_INTERNATIONAL);

  run_next_test();
});

add_test(function test_setCallForward_unconditional() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCallForward = function fakeSetCallForward(options) {
    context.RIL[REQUEST_SET_CALL_FORWARD](0, {});
  };

  context.RIL.setCallForward({
    action: CALL_FORWARD_ACTION_REGISTRATION,
    reason: CALL_FORWARD_REASON_UNCONDITIONAL,
    serviceClass: ICC_SERVICE_CLASS_VOICE,
    number: "666222333",
    timeSeconds: 10
  });

  let postedMessage = workerHelper.postedMessage;

  equal(postedMessage.errorMsg, undefined);

  run_next_test();
});

add_test(function test_queryCallForwardStatus_unconditional() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCallForward = function fakeSetCallForward(options) {
    context.RIL[REQUEST_SET_CALL_FORWARD](0, {});
  };

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.Buf.readString = function fakeReadString() {
    return "+34666222333";
  };

  context.RIL.queryCallForwardStatus = function fakeQueryCallForward(options) {
    context.Buf.int32Array = [
      0,   // rules.timeSeconds
      145, // rules.toa
      49,  // rules.serviceClass
      CALL_FORWARD_REASON_UNCONDITIONAL, // rules.reason
      1,   // rules.active
      1    // rulesLength
    ];
    context.RIL[REQUEST_QUERY_CALL_FORWARD_STATUS](1, {});
  };

  context.RIL.queryCallForwardStatus({
    action: CALL_FORWARD_ACTION_QUERY_STATUS,
    reason: CALL_FORWARD_REASON_UNCONDITIONAL,
    serviceClass: ICC_SERVICE_CLASS_VOICE,
    number: "666222333",
    timeSeconds: 10
  });

  let postedMessage = workerHelper.postedMessage;

  equal(postedMessage.errorMsg, undefined);
  ok(Array.isArray(postedMessage.rules));
  do_print(postedMessage.rules.length);
  equal(postedMessage.rules.length, 1);
  ok(postedMessage.rules[0].active);
  equal(postedMessage.rules[0].reason, CALL_FORWARD_REASON_UNCONDITIONAL);
  equal(postedMessage.rules[0].number, "+34666222333");
  run_next_test();
});
