/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_queryCLIP_provisioned() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.RIL.queryCLIP = function fakeQueryCLIP(options) {
    context.Buf.int32Array = [
      1,  // CLIP provisioned.
      1   // Length.
    ];
    context.RIL[REQUEST_QUERY_CLIP](1, {});
  };

  context.RIL.queryCLIP({});

  let postedMessage = workerHelper.postedMessage;

  equal(postedMessage.errorMsg, undefined);
  equal(postedMessage.provisioned, 1);
  run_next_test();
});

add_test(function test_getCLIP_error_generic_failure_invalid_length() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32 = function fakeReadUint32() {
    return context.Buf.int32Array.pop();
  };

  context.RIL.queryCLIP = function fakeQueryCLIP(options) {
    context.Buf.int32Array = [
      1,  // CLIP provisioned.
      0   // Length.
    ];
    context.RIL[REQUEST_QUERY_CLIP](1, {});
  };

  context.RIL.queryCLIP({});

  let postedMessage = workerHelper.postedMessage;

  equal(postedMessage.errorMsg, GECKO_ERROR_GENERIC_FAILURE);
  run_next_test();
});
