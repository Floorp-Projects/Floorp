/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

// Calling line identification restriction constants.

// Uses subscription default value.
const CLIR_DEFAULT     = 0;
// Restricts CLI presentation.
const CLIR_INVOCATION  = 1;
// Allows CLI presentation.
const CLIR_SUPPRESSION = 2;

function run_test() {
  run_next_test();
}

add_test(function test_setCLIR_success() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCLIR = function fakeSetCLIR(options) {
    context.RIL[REQUEST_SET_CLIR](0, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.setCLIR({
    clirMode: CLIR_DEFAULT
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);

  run_next_test();
});

add_test(function test_setCLIR_generic_failure() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setCLIR = function fakeSetCLIR(options) {
    context.RIL[REQUEST_SET_CLIR](0, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  };

  context.RIL.setCLIR({
    clirMode: CLIR_DEFAULT
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);

  run_next_test();
});

add_test(function test_getCLIR_n0_m1() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.RIL.getCLIR = function fakeGetCLIR(options) {
    context.Buf.int32Array = [
      1,  // Presentation indicator is used according to the subscription
          // of the CLIR service.
      0,  // CLIR provisioned in permanent mode.
      2   // Length.
    ];
    context.RIL[REQUEST_GET_CLIR](1, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.getCLIR({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.n, 0);
  do_check_eq(postedMessage.m, 1);
  run_next_test();
});

add_test(function test_getCLIR_error_generic_failure_invalid_length() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.RIL.getCLIR = function fakeGetCLIR(options) {
    context.Buf.int32Array = [
      1,  // Presentation indicator is used according to the subscription
          // of the CLIR service.
      0,  // CLIR provisioned in permanent mode.
      0   // Length (invalid one).
    ];
    context.RIL[REQUEST_GET_CLIR](1, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.getCLIR({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);
  run_next_test();
});
