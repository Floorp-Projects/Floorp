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

add_test(function test_notification() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  function Call(callIndex, number) {
    this.callIndex = callIndex;
    this.number = number;
  }

  Call.prototype = {
    state: CALL_STATE_DIALING,
    //callIndex: 0,
    toa: 0,
    isMpty: false,
    isMT: false,
    als: 0,
    isVoice: true,
    isVoicePrivacy: false,
    //number: null,
    numberPresentation: 0,
    name: null,
    namePresentation: 0,
    uusInfo: null
  };

  let oneCall = {
    0: new Call(0, '00000')
  };

  let twoCalls = {
    0: new Call(0, '00000'),
    1: new Call(1, '11111')
  };

  function testNotification(calls, code, number, resultNotification,
                            resultCallIndex) {

    let testInfo = {calls: calls, code: code, number: number,
                    resultNotification: resultNotification,
                    resultCallIndex: resultCallIndex};
    do_print('Test case info: ' + JSON.stringify(testInfo));

    // Set current calls.
    worker.RIL._processCalls(calls);

    let notificationInfo = {
      notificationType: 1,  // MT
      code: code,
      index: 0,
      type: 0,
      number: number
    };

    worker.RIL._processSuppSvcNotification(notificationInfo);

    let postedMessage = workerHelper.postedMessage;
    do_check_eq(postedMessage.rilMessageType, 'suppSvcNotification');
    do_check_eq(postedMessage.notification, resultNotification);
    do_check_eq(postedMessage.callIndex, resultCallIndex);

    // Clear all existed calls.
    worker.RIL._processCalls(null);
  }

  testNotification(oneCall, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD, 0);

  testNotification(oneCall, SUPP_SVC_NOTIFICATION_CODE2_RETRIEVED, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED, 0);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD, -1);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_RETRIEVED, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED, -1);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, '00000',
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD, 0);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, '11111',
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD, 1);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, '22222',
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD, -1);

  run_next_test();
});

