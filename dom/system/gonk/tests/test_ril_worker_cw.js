/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

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

add_test(function test_setCallWaiting_success() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.RIL.setCallWaiting = function fakeSetCallWaiting(options) {
    worker.RIL[REQUEST_SET_CALL_WAITING](0, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.setCallWaiting({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);

  run_next_test();
});

add_test(function test_setCallWaiting_generic_failure() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.RIL.setCallWaiting = function fakeSetCallWaiting(options) {
    worker.RIL[REQUEST_SET_CALL_WAITING](0, {
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  };

  worker.RIL.setCallWaiting({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);

  run_next_test();
});

add_test(function test_queryCallWaiting_success_enabled_true() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readUint32 = function fakeReadUint32() {
    return worker.Buf.int32Array.pop();
  };

  worker.RIL.queryCallWaiting = function fakeQueryCallWaiting(options) {
    worker.Buf.int32Array = [
      1,  // serviceClass
      1,  // enabled
      1   // length
    ];
    worker.RIL[REQUEST_QUERY_CALL_WAITING](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.queryCallWaiting({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.length, 1);
  do_check_true(postedMessage.enabled);
  run_next_test();
});

add_test(function test_queryCallWaiting_success_enabled_false() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readUint32 = function fakeReadUint32() {
    return worker.Buf.int32Array.pop();
  };

  worker.RIL.queryCallWaiting = function fakeQueryCallWaiting(options) {
    worker.Buf.int32Array = [
      1,  // serviceClass
      0,  // enabled
      1   // length
    ];
    worker.RIL[REQUEST_QUERY_CALL_WAITING](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.queryCallWaiting({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.length, 1);
  do_check_false(postedMessage.enabled);
  run_next_test();
});
