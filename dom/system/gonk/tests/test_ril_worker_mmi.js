/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function parseMMI(mmi) {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });
  return worker.RIL._parseMMI(mmi);
}

function testSendMMI(mmi, error) {
  let postedMessage;
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
    },
    postMessage: function fakePostMessage(message) {
      postedMessage = message;
    }
  })
  worker.RIL.sendMMI({mmi: mmi});

  do_check_eq(postedMessage.rilMessageType, "sendMMI");
  do_check_eq(postedMessage.errorMsg, error);
}

add_test(function test_parseMMI_empty() {
  let mmi = parseMMI("");

  do_check_null(mmi);

  run_next_test();
});

add_test(function test_parseMMI_undefined() {
  let mmi = parseMMI();

  do_check_null(mmi);

  run_next_test();
});

add_test(function test_parseMMI_invalid() {
  let mmi = parseMMI("**");

  do_check_null(mmi);

  run_next_test();
});

add_test(function test_parseMMI_dial_string() {
  let mmi = parseMMI("12345");

  do_check_null(mmi);

  run_next_test();
});

add_test(function test_parseMMI_USSD() {
  let mmi = parseMMI("*123#");

  do_check_eq(mmi.fullMMI, "*123#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, undefined);
  do_check_eq(mmi.sib, undefined);
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_sia() {
  let mmi = parseMMI("*123*1#");

  do_check_eq(mmi.fullMMI, "*123*1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "1");
  do_check_eq(mmi.sib, undefined);
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_sib() {
  let mmi = parseMMI("*123**1#");

  do_check_eq(mmi.fullMMI, "*123**1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "");
  do_check_eq(mmi.sib, "1");
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_sic() {
  let mmi = parseMMI("*123***1#");

  do_check_eq(mmi.fullMMI, "*123***1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "");
  do_check_eq(mmi.sib, "");
  do_check_eq(mmi.sic, "1");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_sia_sib() {
  let mmi = parseMMI("*123*1*1#");

  do_check_eq(mmi.fullMMI, "*123*1*1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "1");
  do_check_eq(mmi.sib, "1");
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_sia_sic() {
  let mmi = parseMMI("*123*1**1#");

  do_check_eq(mmi.fullMMI, "*123*1**1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "1");
  do_check_eq(mmi.sib, "");
  do_check_eq(mmi.sic, "1");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_sib_sic() {
  let mmi = parseMMI("*123**1*1#");

  do_check_eq(mmi.fullMMI, "*123**1*1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "");
  do_check_eq(mmi.sib, "1");
  do_check_eq(mmi.sic, "1");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_pwd() {
  let mmi = parseMMI("*123****1#");

  do_check_eq(mmi.fullMMI, "*123****1#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, "");
  do_check_eq(mmi.sib, "");
  do_check_eq(mmi.sic, "");
  do_check_eq(mmi.pwd, "1");
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_dial_number() {
  let mmi = parseMMI("*123#345");

  do_check_eq(mmi.fullMMI, "*123#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "123");
  do_check_eq(mmi.sia, undefined);
  do_check_eq(mmi.sib, undefined);
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "345");

  run_next_test();
});


/**
 * MMI procedures tests
 */

add_test(function test_parseMMI_activation() {
  let mmi = parseMMI("*00*12*34*56#");

  do_check_eq(mmi.fullMMI, "*00*12*34*56#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  do_check_eq(mmi.serviceCode, "00");
  do_check_eq(mmi.sia, "12");
  do_check_eq(mmi.sib, "34");
  do_check_eq(mmi.sic, "56");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_deactivation() {
  let mmi = parseMMI("#00*12*34*56#");

  do_check_eq(mmi.fullMMI, "#00*12*34*56#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_DEACTIVATION);
  do_check_eq(mmi.serviceCode, "00");
  do_check_eq(mmi.sia, "12");
  do_check_eq(mmi.sib, "34");
  do_check_eq(mmi.sic, "56");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_interrogation() {
  let mmi = parseMMI("*#00*12*34*56#");

  do_check_eq(mmi.fullMMI, "*#00*12*34*56#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_INTERROGATION);
  do_check_eq(mmi.serviceCode, "00");
  do_check_eq(mmi.sia, "12");
  do_check_eq(mmi.sib, "34");
  do_check_eq(mmi.sic, "56");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_registration() {
  let mmi = parseMMI("**00*12*34*56#");

  do_check_eq(mmi.fullMMI, "**00*12*34*56#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_REGISTRATION);
  do_check_eq(mmi.serviceCode, "00");
  do_check_eq(mmi.sia, "12");
  do_check_eq(mmi.sib, "34");
  do_check_eq(mmi.sic, "56");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

add_test(function test_parseMMI_erasure() {
  let mmi = parseMMI("##00*12*34*56#");

  do_check_eq(mmi.fullMMI, "##00*12*34*56#");
  do_check_eq(mmi.procedure, MMI_PROCEDURE_ERASURE);
  do_check_eq(mmi.serviceCode, "00");
  do_check_eq(mmi.sia, "12");
  do_check_eq(mmi.sib, "34");
  do_check_eq(mmi.sic, "56");
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, "");

  run_next_test();
});

/**
 * sendMMI tests.
 */

add_test(function test_sendMMI_empty() {
  testSendMMI("", "NO_VALID_MMI_STRING");

  run_next_test();
});

add_test(function test_sendMMI_undefined() {
  testSendMMI({}, "NO_VALID_MMI_STRING");

  run_next_test();
});

add_test(function test_sendMMI_invalid() {
  testSendMMI("**", "NO_VALID_MMI_STRING");

  run_next_test();
});

add_test(function test_sendMMI_dial_string() {
  testSendMMI("123", "NO_VALID_MMI_STRING");

  run_next_test();
});

add_test(function test_sendMMI_call_forwarding() {
  // TODO: Bug 793192 - MMI Codes: support call forwarding
  testSendMMI("*21#", "CALL_FORWARDING_NOT_SUPPORTED_VIA_MMI");

  run_next_test();
});

add_test(function test_sendMMI_sim_function() {
  // TODO: Bug 793187 - MMI Codes: Support PIN/PIN2/PUK handling via MMI codes
  testSendMMI("*04#", "SIM_FUNCTION_NOT_SUPPORTED_VIA_MMI");

  run_next_test();
});

add_test(function test_sendMMI_get_IMEI() {
  // TODO: Bug 793189 - MMI Codes: get IMEI
  testSendMMI("*#06#", "GET_IMEI_NOT_SUPPORTED_VIA_MMI");

  run_next_test();
});

add_test(function test_sendMMI_call_barring() {
  testSendMMI("*33#", "CALL_BARRING_NOT_SUPPORTED_VIA_MMI");

  run_next_test();
});

add_test(function test_sendMMI_call_waiting() {
  testSendMMI("*43#", "CALL_WAITING_NOT_SUPPORTED_VIA_MMI");

  run_next_test();
});

add_test(function test_sendMMI_USSD() {
  let postedMessage;
  let ussdOptions;
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
    },
    postMessage: function fakePostMessage(message) {
      postedMessage = message;
    },
  });

  worker.RIL.sendUSSD = function fakeSendUSSD(options){
    ussdOptions = options;
    worker.RIL[REQUEST_SEND_USSD](0, {
      rilRequestError: ERROR_SUCCESS
    });
  }

  worker.RIL.sendMMI({mmi: "*123#"});

  do_check_eq(ussdOptions.ussd, "*123#");
  do_check_eq (postedMessage.errorMsg, GECKO_ERROR_SUCCESS);
  do_check_true(postedMessage.success);
  do_check_true(worker.RIL._ussdSession);

  run_next_test();
});

add_test(function test_sendMMI_USSD_error() {
  let postedMessage;
  let ussdOptions;
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
    },
    postMessage: function fakePostMessage(message) {
      postedMessage = message;
    },
  });

  worker.RIL.sendUSSD = function fakeSendUSSD(options){
    ussdOptions = options;
    worker.RIL[REQUEST_SEND_USSD](0, {
      rilRequestError: ERROR_GENERIC_FAILURE
    });
  }

  worker.RIL.sendMMI({mmi: "*123#"});

  do_check_eq(ussdOptions.ussd, "*123#");
  do_check_eq (postedMessage.errorMsg, GECKO_ERROR_GENERIC_FAILURE);
  do_check_false(postedMessage.success);
  do_check_false(worker.RIL._ussdSession);

  run_next_test();
});
