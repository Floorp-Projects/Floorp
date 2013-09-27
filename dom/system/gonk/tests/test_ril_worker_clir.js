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

function _getWorker() {
  let _postedMessage;
  let _worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
    },
    postMessage: function fakePostMessage(message) {
      _postedMessage = message;
    }
  });
  return {
    get postedMessage() {
      return _postedMessage;
    },
    get worker() {
      return _worker;
    }
  };
}

add_test(function test_setCLIR_success() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.RIL.setCLIR = function fakeSetCLIR(options) {
    worker.RIL[REQUEST_SET_CLIR](0, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.setCLIR({
    clirMode: CLIR_DEFAULT
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);

  run_next_test();
});

add_test(function test_setCLIR_generic_failure() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.RIL.setCLIR = function fakeSetCLIR(options) {
    worker.RIL[REQUEST_SET_CLIR](0, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  };

  worker.RIL.setCLIR({
    clirMode: CLIR_DEFAULT
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);

  run_next_test();
});

add_test(function test_getCLIR_n0_m1() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readInt32 = function fakeReadUint32() {
    return worker.Buf.int32Array.pop();
  };

  worker.RIL.getCLIR = function fakeGetCLIR(options) {
    worker.Buf.int32Array = [
      1,  // Presentation indicator is used according to the subscription
          // of the CLIR service.
      0,  // CLIR provisioned in permanent mode.
      2   // Length.
    ];
    worker.RIL[REQUEST_GET_CLIR](1, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.getCLIR({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.n, 0);
  do_check_eq(postedMessage.m, 1);
  run_next_test();
});

add_test(function test_getCLIR_error_generic_failure_invalid_length() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readInt32 = function fakeReadUint32() {
    return worker.Buf.int32Array.pop();
  };

  worker.RIL.getCLIR = function fakeGetCLIR(options) {
    worker.Buf.int32Array = [
      1,  // Presentation indicator is used according to the subscription
          // of the CLIR service.
      0,  // CLIR provisioned in permanent mode.
      0   // Length (invalid one).
    ];
    worker.RIL[REQUEST_GET_CLIR](1, {
      rilMessageType: "setCLIR",
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.getCLIR({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);
  run_next_test();
});
