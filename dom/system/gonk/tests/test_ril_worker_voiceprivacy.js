/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function _getWorker() {
  let _postedMessage;
  let _worker = newWorker({
    postRILMessage: function(data) {
    },
    postMessage: function(message) {
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

add_test(function test_setVoicePrivacyMode_success() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.RIL.setVoicePrivacyMode = function fakeSetVoicePrivacyMode(options) {
    worker.RIL[REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE](0, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.setVoicePrivacyMode({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);

  run_next_test();
});

add_test(function test_setVoicePrivacyMode_generic_failure() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.RIL.setVoicePrivacyMode = function fakeSetVoicePrivacyMode(options) {
    worker.RIL[REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE](0, {
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  };

  worker.RIL.setVoicePrivacyMode({
    enabled: true
  });

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, "GenericFailure");

  run_next_test();
});

add_test(function test_queryVoicePrivacyMode_success_enabled_true() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readInt32List = function fakeReadUint32List() {
    return [1];
  };

  worker.RIL.queryVoicePrivacyMode = function fakeQueryVoicePrivacyMode(options) {
    worker.RIL[REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.queryVoicePrivacyMode();

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_true(postedMessage.enabled);
  run_next_test();
});

add_test(function test_queryVoicePrivacyMode_success_enabled_false() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  worker.Buf.readInt32List = function fakeReadUint32List() {
    return [0];
  };

  worker.RIL.queryVoicePrivacyMode = function fakeQueryVoicePrivacyMode(options) {
    worker.RIL[REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE](1, {
      rilRequestError: ERROR_SUCCESS
    });
  };

  worker.RIL.queryVoicePrivacyMode();

  let postedMessage = workerHelper.postedMessage;

  do_check_eq(postedMessage.errorMsg, undefined);
  do_check_false(postedMessage.enabled);
  run_next_test();
});
