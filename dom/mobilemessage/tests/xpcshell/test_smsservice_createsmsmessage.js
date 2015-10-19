/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ICC_ID = "123456789";

function do_check_throws(f, result, stack) {
  if (!stack) {
    stack = Components.stack.caller;
  }

  try {
    f();
  } catch (exc) {
    if (exc.result == result)
      return;
    do_throw("expected result " + result + ", caught " + exc, stack);
  }
  do_throw("expected result " + result + ", none thrown", stack);
}

var gMobileMessageService = Cc["@mozilla.org/mobilemessage/mobilemessageservice;1"]
                            .getService(Ci.nsIMobileMessageService);
function newMessage() {
  return gMobileMessageService.createSmsMessage.apply(gMobileMessageService, arguments);
}

function run_test() {
  run_next_test();
}

/**
 * Ensure an SmsMessage object created has sensible initial values.
 */
add_test(function test_interface() {
  let sms = newMessage(null, null, ICC_ID, "sent", "success", null, null, null,
                       "normal", Date.now(), Date.now(), Date.now(), true);
  do_check_true(sms instanceof Ci.nsISmsMessage);
  do_check_eq(sms.id, 0);
  do_check_eq(sms.threadId, 0);
  do_check_eq(sms.iccId, ICC_ID);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.receiver, null);
  do_check_eq(sms.sender, null);
  do_check_eq(sms.body, null);
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test if ICC ID is null when it's not available.
 */
add_test(function test_icc_id_not_available() {
  let sms = newMessage(null, null, null, "sent", "success", null, null, null,
                       "normal", Date.now(), Date.now(), Date.now(), true);
  do_check_true(sms instanceof Ci.nsISmsMessage);
  do_check_eq(sms.id, 0);
  do_check_eq(sms.threadId, 0);
  do_check_eq(sms.iccId, null);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.receiver, null);
  do_check_eq(sms.sender, null);
  do_check_eq(sms.body, null);
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Verify that attributes are read-only.
 */
add_test(function test_readonly_attributes() {
  let sms = newMessage(null, null, ICC_ID, "sent", "success", null, null, null,
                       "normal", Date.now(), Date.now(), Date.now(), true);

  sms.id = 1;
  do_check_eq(sms.id, 0);

  sms.threadId = 1;
  do_check_eq(sms.threadId, 0);

  sms.iccId = "987654321";
  do_check_eq(sms.iccId, ICC_ID);

  sms.delivery = "received";
  do_check_eq(sms.delivery, "sent");

  sms.deliveryStatus = "pending";
  do_check_eq(sms.deliveryStatus, "success");

  sms.receiver = "a receiver";
  do_check_eq(sms.receiver, null);

  sms.sender = "a sender";
  do_check_eq(sms.sender, null);

  sms.body = "a body";
  do_check_eq(sms.body, null);

  sms.messageClass = "class-0";
  do_check_eq(sms.messageClass, "normal");

  let oldTimestamp = sms.timestamp;
  sms.timestamp = Date.now();
  do_check_eq(sms.timestamp, oldTimestamp);

  let oldSentTimestamp = sms.sentTimestamp;
  sms.sentTimestamp = Date.now();
  do_check_eq(sms.sentTimestamp, oldSentTimestamp);

  let oldDeliveryTimestamp = sms.deliveryTimestamp;
  sms.deliveryTimestamp = Date.now();
  do_check_eq(sms.deliveryTimestamp, oldDeliveryTimestamp);

  sms.read = false;
  do_check_true(sms.read);

  run_next_test();
});

/**
 * Test supplying the timestamp as a number of milliseconds.
 */
add_test(function test_timestamp_number() {
  let ts = Date.now();
  let sms = newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
                       "the body", "normal", ts, ts, ts, true);
  do_check_eq(sms.id, 42);
  do_check_eq(sms.threadId, 1);
  do_check_eq(sms.iccId, ICC_ID);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.sender, "the sender");
  do_check_eq(sms.receiver, "the receiver");
  do_check_eq(sms.body, "the body");
  do_check_eq(sms.messageClass, "normal");
  do_check_eq(sms.timestamp, ts);
  do_check_eq(sms.sentTimestamp, ts);
  do_check_eq(sms.deliveryTimestamp, ts);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test supplying the timestamp as a Date object, which will be automatically
 * casted to unsigned long long.
 */
add_test(function test_timestamp_date() {
  let date = new Date();
  let sms = newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
                       "the body", "normal", date, date, date, true);
  do_check_eq(sms.id, 42);
  do_check_eq(sms.threadId, 1);
  do_check_eq(sms.iccId, ICC_ID);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.sender, "the sender");
  do_check_eq(sms.receiver, "the receiver");
  do_check_eq(sms.body, "the body");
  do_check_eq(sms.messageClass, "normal");
  do_check_eq(sms.timestamp, date.getTime());
  do_check_eq(sms.sentTimestamp, date.getTime());
  do_check_eq(sms.deliveryTimestamp, date.getTime());
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test that an invalid delivery string is not accepted.
 */
add_test(function test_invalid_delivery_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "this is invalid", "pending", "the sender",
               "the receiver", "the body", "normal", Date.now(), 0, 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'delivery' argument.
 */
add_test(function test_invalid_delivery_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, 1, "pending", "the sender", "the receiver", "the body",
               "normal", Date.now(), 0, 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid delivery status string is not accepted.
 */
add_test(function test_invalid_delivery_status_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "this is invalid", "the sender", "the receiver",
               "the body", "normal", Date.now(), Date.now(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'deliveryStatus' argument.
 */
add_test(function test_invalid_delivery_status_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", 1, "the sender", "the receiver", "the body",
               "normal", Date.now(), Date.now(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid message class string is not accepted.
 */
add_test(function test_invalid_message_class_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "this is invalid", Date.now(), Date.now(), Date.now(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'messageClass' argument.
 */
add_test(function test_invalid_message_class_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", 1, Date.now(), Date.now(), Date.now(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});
