/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function newMmsTransactionHelper() {
  let MMS_Service = {};
  subscriptLoader.loadSubScript("resource://gre/components/MmsService.js", MMS_Service);
  MMS_Service.debug = do_print;
  return MMS_Service.gMmsTransactionHelper;
}
var CallFunc = newMmsTransactionHelper();
function run_test() {
  run_next_test();
}

//
// Test target: checkMaxValuesParameters
//

//// Test Long Subject field ////
//// @see OMA-ETS-MMS_CON-V1_3-20080128-C 5.1.1.1.10 MMS-1.3-con-171 ////

add_test(function test_LongSubjectField() {
  let msg = {};
  msg.headers = {};
  // Test for normal TextString
  msg.headers["subject"] = "abcdefghijklmnopqrstuvwxyz0123456789/-+@?";
  do_check_eq(CallFunc.checkMaxValuesParameters(msg), false);

  run_next_test();
});

//// Test recipient field length

add_test(function test_LongRecipientField() {
  let msg = {};
  msg.headers = {};
  msg.headers["to"] = [
    { address:
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz0123456789/-+@?" +
      "abcdefghijklmnopqrstuvwxyz",
      type: "PLMN" },
  ];
  do_check_eq(CallFunc.checkMaxValuesParameters(msg), false);

  run_next_test();
});


add_test(function test_checkMaxValuesParameters() {
  let msg = {};
  msg.headers = {};
  msg.headers["cc"] = [
    { address: "+789", type: "PLMN" },
    { address: "+119", type: "num" },
    { address: "Joe2 User " +
               "<abcdefghijklmnopqrstuvwxyz0123456789" +
               "abcdefghijklmnopqrstuvwxyz0123456789@" +
               "abcdefghijklmnopqrstuvwxyz0123456789" +
               "abcdefghijklmnopqrstuvwxyz0123456789." +
               "abcdefghijklmnopqrstuvwxyz0123456789" +
               "abcdefghijklmnopqrstuvwxyz0123456789" +
               "abcdefghijklmnopqrstuvwxyz0123456789->"
      , type: "email" },
  ];
  do_check_eq(CallFunc.checkMaxValuesParameters(msg), false);

  run_next_test();
});

//// Test total recipient count over 20

add_test(function test_TotalRecipientCount() {
  let msg = {};
  msg.headers = {};
  do_check_eq(CallFunc.checkMaxValuesParameters(msg), false);

  msg.headers["to"] = [
    { address: "+123", type: "PLMN" },
    { address: "+456", type: "num" },
    { address: "Joe User <joe@user.org>", type: "email" },
    { address: "+123", type: "PLMN" },
    { address: "+456", type: "num" },
    { address: "Joe User <joe@user.org>", type: "email" },
    { address: "+123", type: "PLMN" },
  ];
  msg.headers["cc"] = [
    { address: "+789", type: "PLMN" },
    { address: "+119", type: "num" },
    { address: "Joe2 User <joe2@user.org>", type: "email" },
    { address: "+789", type: "PLMN" },
    { address: "+119", type: "num" },
    { address: "Joe2 User <joe2@user.org>", type: "email" },
    { address: "+789", type: "PLMN" },
  ];
  msg.headers["bcc"] = [
    { address: "+110", type: "num" },
    { address: "Joe3 User <joe2@user.org>", type: "email" },
    { address: "Joe2 User <joe2@user.org>", type: "email" },
    { address: "+789", type: "PLMN" },
    { address: "+119", type: "num" },
    { address: "Joe2 User <joe2@user.org>", type: "email" },
    { address: "+789", type: "PLMN" },
  ];
  do_check_eq(CallFunc.checkMaxValuesParameters(msg), false);

  run_next_test();
});

////Test name parameter in content-type field of multi-parts

add_test(function test_NameParameterInContentType() {
  let msg = {};
  msg.headers = {};
  msg.headers["to"] = [
                       { address: "Joe User <joe@user.org>", type: "email" },
                      ];
  let params = {};
  params["name"] = "abcdefghijklmnopqrstuvwxyz0123456789/-+@?";
  let headers = {};
  headers["content-type"] = {
    media: null,
    params: params,
  };
  msg.parts = new Array(1);
  msg.parts[0] = {
    index: 0,
    headers: headers,
    content: null,
  };
  do_check_eq(CallFunc.checkMaxValuesParameters(msg), false);

  run_next_test();
});