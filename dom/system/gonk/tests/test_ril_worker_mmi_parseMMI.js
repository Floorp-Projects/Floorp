/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

let worker;
function parseMMI(mmi) {
  if (!worker) {
    worker = newWorker({
      postRILMessage: function(data) {
        // Do nothing
      },
      postMessage: function(message) {
        // Do nothing
      }
    });
  }

  let context = worker.ContextPool._contexts[0];
  return context.RIL._parseMMI(mmi);
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

add_test(function test_parseMMI_one_digit_short_code() {
  let mmi = parseMMI("1");

  do_check_eq(mmi.fullMMI, "1");
  do_check_eq(mmi.procedure, undefined);
  do_check_eq(mmi.serviceCode, undefined);
  do_check_eq(mmi.sia, undefined);
  do_check_eq(mmi.sib, undefined);
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_invalid_short_code() {
  let mmi = parseMMI("11");

  do_check_null(mmi);

  run_next_test();
});

add_test(function test_parseMMI_short_code() {
  let mmi = parseMMI("21");

  do_check_eq(mmi.fullMMI, "21");
  do_check_eq(mmi.procedure, undefined);
  do_check_eq(mmi.serviceCode, undefined);
  do_check_eq(mmi.sia, undefined);
  do_check_eq(mmi.sib, undefined);
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_dial_string() {
  let mmi = parseMMI("12345");

  do_check_null(mmi);

  run_next_test();
});

add_test(function test_parseMMI_USSD_without_asterisk_prefix() {
  let mmi = parseMMI("123#");

  do_check_eq(mmi.fullMMI, "123#");
  do_check_eq(mmi.procedure, undefined);
  do_check_eq(mmi.serviceCode, undefined);
  do_check_eq(mmi.sia, undefined);
  do_check_eq(mmi.sib, undefined);
  do_check_eq(mmi.sic, undefined);
  do_check_eq(mmi.pwd, undefined);
  do_check_eq(mmi.dialNumber, undefined);

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
