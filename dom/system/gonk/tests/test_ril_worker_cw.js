/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_setCallWaiting_success() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCallWaiting = function fakeSetCallWaiting(options) {
    context.RIL[REQUEST_SET_CALL_WAITING](0, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.setCallWaiting({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);

  run_next_test();
});

add_test(function test_setCallWaiting_generic_failure() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCallWaiting = function fakeSetCallWaiting(options) {
    context.RIL[REQUEST_SET_CALL_WAITING](0, {
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  };

  context.RIL.setCallWaiting({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);

  run_next_test();
});

add_test(function test_queryCallWaiting_success_enabled_true() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.RIL.queryCallWaiting = function fakeQueryCallWaiting(options) {
    context.Buf.int32Array = [
      1,  // serviceClass
      1,  // enabled
      1   // length
    ];
    context.RIL[REQUEST_QUERY_CALL_WAITING](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.queryCallWaiting({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.length, 1);
  do_check_true(postedMessage.enabled);
  run_next_test();
});

add_test(function test_queryCallWaiting_success_enabled_false() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.RIL.queryCallWaiting = function fakeQueryCallWaiting(options) {
    context.Buf.int32Array = [
      1,  // serviceClass
      0,  // enabled
      1   // length
    ];
    context.RIL[REQUEST_QUERY_CALL_WAITING](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.queryCallWaiting({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.length, 1);
  do_check_false(postedMessage.enabled);
  run_next_test();
});
