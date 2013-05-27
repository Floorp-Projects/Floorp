/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

let gMobileMessageService = Cc["@mozilla.org/mobilemessage/mobilemessageservice;1"]
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
  let sms = newMessage(null, null, "sent", "pending", null, null, null,
                       "normal", new Date(), true);
  do_check_true(sms instanceof Ci.nsIDOMMozSmsMessage);
  do_check_eq(sms.id, 0);
  do_check_eq(sms.threadId, 0);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "pending");
  do_check_eq(sms.receiver, null);
  do_check_eq(sms.sender, null);
  do_check_eq(sms.body, null);
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Verify that attributes are read-only.
 */
add_test(function test_readonly_attributes() {
  let sms = newMessage(null, null, "received", "success", null, null, null,
                       "normal", new Date(), true);

  sms.id = 1;
  do_check_eq(sms.id, 0);

  sms.threadId = 1;
  do_check_eq(sms.threadId, 0);

  sms.delivery = "sent";
  do_check_eq(sms.delivery, "received");

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

  let oldTimestamp = sms.timestamp.getTime();
  sms.timestamp = new Date();
  do_check_eq(sms.timestamp.getTime(), oldTimestamp);

  sms.read = false;
  do_check_true(sms.read);

  run_next_test();
});

/**
 * Test supplying the timestamp as a number of milliseconds.
 */
add_test(function test_timestamp_number() {
  let ts = Date.now();
  let sms = newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
                       "the body", "normal", ts, true);
  do_check_eq(sms.id, 42);
  do_check_eq(sms.threadId, 1);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "pending");
  do_check_eq(sms.sender, "the sender");
  do_check_eq(sms.receiver, "the receiver");
  do_check_eq(sms.body, "the body");
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_eq(sms.timestamp.getTime(), ts);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test supplying the timestamp as a Date object.
 */
add_test(function test_timestamp_date() {
  let date = new Date();
  let sms = newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
                       "the body", "normal", date, true);
  do_check_eq(sms.id, 42);
  do_check_eq(sms.threadId, 1);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "pending");
  do_check_eq(sms.sender, "the sender");
  do_check_eq(sms.receiver, "the receiver");
  do_check_eq(sms.body, "the body");
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_eq(sms.timestamp.getTime(), date.getTime());
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test that a floating point number for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_float() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
               "the body", "normal", 3.1415, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a null value for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_null() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
               "the body", "normal", null, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that undefined for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_undefined() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
               "the body", "normal", undefined, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a random object for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_object() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
               "the body", "normal", {}, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid delivery string is not accepted.
 */
add_test(function test_invalid_delivery_string() {
  do_check_throws(function() {
    newMessage(42, 1, "this is invalid", "pending", "the sender",
               "the receiver", "the body", "normal", new Date(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'delivery' argument.
 */
add_test(function test_invalid_delivery_string() {
  do_check_throws(function() {
    newMessage(42, 1, 1, "pending", "the sender", "the receiver", "the body",
               "normal", new Date(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid delivery status string is not accepted.
 */
add_test(function test_invalid_delivery_status_string() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "this is invalid", "the sender", "the receiver",
               "the body", "normal", new Date(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'deliveryStatus' argument.
 */
add_test(function test_invalid_delivery_status_string() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", 1, "the sender", "the receiver", "the body",
               "normal", new Date(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid message class string is not accepted.
 */
add_test(function test_invalid_message_class_string() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
               "the body", "this is invalid", new Date(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'messageClass' argument.
 */
add_test(function test_invalid_message_class_string() {
  do_check_throws(function() {
    newMessage(42, 1, "sent", "pending", "the sender", "the receiver",
               "the body", 1, new Date(), true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});
