/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_setVoicePrivacyMode_success() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setVoicePrivacyMode = function fakeSetVoicePrivacyMode(options) {
    context.RIL[REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE](0, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.setVoicePrivacyMode({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);

  run_next_test();
});

add_test(function test_setVoicePrivacyMode_generic_failure() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.RIL.setVoicePrivacyMode = function fakeSetVoicePrivacyMode(options) {
    context.RIL[REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE](0, {
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  };

  context.RIL.setVoicePrivacyMode({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");

  run_next_test();
});

add_test(function test_queryVoicePrivacyMode_success_enabled_true() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32List = function fakeReadUint32List() {
    return [1];
  };

  context.RIL.queryVoicePrivacyMode = function fakeQueryVoicePrivacyMode(options) {
    context.RIL[REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.queryVoicePrivacyMode();

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.enabled);
  run_next_test();
});

add_test(function test_queryVoicePrivacyMode_success_enabled_false() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.Buf.readInt32List = function fakeReadUint32List() {
    return [0];
  };

  context.RIL.queryVoicePrivacyMode = function fakeQueryVoicePrivacyMode(options) {
    context.RIL[REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  context.RIL.queryVoicePrivacyMode();

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_false(postedMessage.enabled);
  run_next_test();
});
