/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_notification() {
  let workerHelper = newInterceptWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  function Call(callIndex, number) {
    this.callIndex = callIndex;
    this.number = number;
  }

  Call.prototype = {
    // Should use CALL_STATE_ACTIVE.
    // Any new outgoing call (state = dialing or alerting) will be drop if there
    // is no pending outgoing call created before.
    state: CALL_STATE_ACTIVE,
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

  function testNotification(calls, code, number, resultNotification) {

    let testInfo = {calls: calls, code: code, number: number,
                    resultNotification: resultNotification};
    do_print('Test case info: ' + JSON.stringify(testInfo));

    // Set current calls.
    context.RIL.sendChromeMessage({
      rilMessageType: "currentCalls",
      calls: calls
    });

    let notificationInfo = {
      notificationType: 1,  // MT
      code: code,
      index: 0,
      type: 0,
      number: number
    };

    context.RIL._processSuppSvcNotification(notificationInfo);

    let postedMessage = workerHelper.postedMessage;
    equal(postedMessage.rilMessageType, 'suppSvcNotification');
    equal(postedMessage.number, number);
    equal(postedMessage.notification, resultNotification);

    // Clear all existed calls.
    context.RIL.sendChromeMessage({
      rilMessageType: "currentCalls",
      calls: {}
    });
  }

  testNotification(oneCall, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD);

  testNotification(oneCall, SUPP_SVC_NOTIFICATION_CODE2_RETRIEVED, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_RETRIEVED, null,
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, '00000',
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, '11111',
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD);

  testNotification(twoCalls, SUPP_SVC_NOTIFICATION_CODE2_PUT_ON_HOLD, '22222',
                   GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD);

  run_next_test();
});
