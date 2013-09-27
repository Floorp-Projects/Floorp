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

add_test(function test_queryCLIP_provisioned() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readInt32 = function fakeReadUint32() {
    return worker.Buf.int32Array.pop();
  };

  worker.RIL.queryCLIP = function fakeQueryCLIP(options) {
    worker.Buf.int32Array = [
      1,  // CLIP provisioned.
      1   // Length.
    ];
    worker.RIL[REQUEST_QUERY_CLIP](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.queryCLIP({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.success);
  do_check_eq(postedMessage.provisioned, 1);
  run_next_test();
});

add_test(function test_getCLIP_error_generic_failure_invalid_length() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readInt32 = function fakeReadUint32() {
    return worker.Buf.int32Array.pop();
  };

  worker.RIL.queryCLIP = function fakeQueryCLIP(options) {
    worker.Buf.int32Array = [
      1,  // CLIP provisioned.
      0   // Length.
    ];
    worker.RIL[REQUEST_QUERY_CLIP](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.queryCLIP({});

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");
  do_check_false(postedMessage.success);
  run_next_test();
});
