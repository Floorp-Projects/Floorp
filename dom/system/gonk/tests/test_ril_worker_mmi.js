/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function createMMIOptions(procedure, serviceCode, sia, sib, sic) {
  let mmi = {
    fullMMI: Array.slice(arguments).join("*") + "#",
    procedure: procedure,
    serviceCode: serviceCode,
    sia: sia,
    sib: sib,
    sic: sic
  };

  return mmi;
}

function testSendMMI(mmi, error) {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];

  do_print("worker.postMessage " + worker.postMessage);

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({rilMessageType: "sendMMI", mmi: mmi});

  let postedMessage = workerhelper.postedMessage;

  equal(postedMessage.rilMessageType, "sendMMI");
  equal(postedMessage.errorMsg, error);
}

/**
 * sendMMI tests.
 */

add_test(function test_sendMMI_null() {
  testSendMMI(null, MMI_ERROR_KS_ERROR);

  run_next_test();
});

add_test(function test_sendMMI_short_code() {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];

  let ussdOptions;

  context.RIL.sendUSSD = function fakeSendUSSD(options){
    ussdOptions = options;
    context.RIL[REQUEST_SEND_USSD](0, {});
  };

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({mmi: {fullMMI: "**"}});

  let postedMessage = workerhelper.postedMessage;
  equal(ussdOptions.ussd, "**");
  equal(postedMessage.errorMsg, undefined);
  ok(context.RIL._ussdSession);

  run_next_test();
});

add_test(function test_sendMMI_USSD() {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];
  let ussdOptions;

  context.RIL.sendUSSD = function fakeSendUSSD(options) {
    ussdOptions = options;
    context.RIL[REQUEST_SEND_USSD](0, {});
  };

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({mmi: createMMIOptions("*", "123")});

  let postedMessage = workerhelper.postedMessage;

  equal(ussdOptions.ussd, "**123#");
  equal(postedMessage.errorMsg, undefined);
  ok(context.RIL._ussdSession);

  run_next_test();
});

add_test(function test_sendMMI_USSD_error() {
  let workerhelper = newInterceptWorker();
  let worker = workerhelper.worker;
  let context = worker.ContextPool._contexts[0];
  let ussdOptions;

  context.RIL.sendUSSD = function fakeSendUSSD(options){
    ussdOptions = options;
    context.RIL[REQUEST_SEND_USSD](0, {
      errorMsg: GECKO_ERROR_GENERIC_FAILURE
    });
  };

  context.RIL.radioState = GECKO_RADIOSTATE_ENABLED;
  context.RIL.sendMMI({mmi: createMMIOptions("*", "123")});

  let postedMessage = workerhelper.postedMessage;

  equal(ussdOptions.ussd, "**123#");
  equal(postedMessage.errorMsg, GECKO_ERROR_GENERIC_FAILURE);
  ok(!context.RIL._ussdSession);

  run_next_test();
});
