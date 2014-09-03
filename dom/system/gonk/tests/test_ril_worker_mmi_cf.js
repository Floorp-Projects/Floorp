/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function setCallForwardSuccess(mmi) {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCallForward = function fakeSetCallForward(options) {
    context.RIL[REQUEST_SET_CALL_FORWARD](0, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({mmi: mmi});

  let postedMessage = workerhelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, GECKO_ERROR_SUCCESS);
  do_check_true(postedMessage.success);
}

add_test(function test_sendMMI_call_forwarding_activation() {
  setCallForwardSuccess("*21*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_deactivation() {
  setCallForwardSuccess("#21*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_interrogation() {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];

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
    context.RIL[REQUEST_QUERY_CALL_FORWARD_STATUS](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({mmi: "*#21#"});

  let postedMessage = workerhelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, GECKO_ERROR_SUCCESS);
  do_check_true(postedMessage.success);
  do_check_true(Array.isArray(postedMessage.rules));
  do_check_eq(postedMessage.rules.length, 1);
  do_check_true(postedMessage.rules[0].active);
  do_check_eq(postedMessage.rules[0].reason, CALL_FORWARD_REASON_UNCONDITIONAL);
  do_check_eq(postedMessage.rules[0].number, "+34666222333");
  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_interrogation_no_rules() {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return 0;
  };

  context.RIL.queryCallForwardStatus = function fakeQueryCallForward(options) {
    context.RIL[REQUEST_QUERY_CALL_FORWARD_STATUS](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({mmi: "*#21#"});

  let postedMessage = workerhelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, GECKO_ERROR_GENERIC_FAILURE);
  do_check_false(postedMessage.success);

  run_next_test();
});


add_test(function test_sendMMI_call_forwarding_registration() {
  setCallForwardSuccess("**21*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_erasure() {
  setCallForwardSuccess("##21*12345*99#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_CFB() {
  setCallForwardSuccess("*67*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_CFNRy() {
  setCallForwardSuccess("*61*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_CFNRc() {
  setCallForwardSuccess("*62*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_CFAll() {
  setCallForwardSuccess("*004*12345*99*10#");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding_CFAllConditional() {
  setCallForwardSuccess("*002*12345*99*10#");

  run_next_test();
});
