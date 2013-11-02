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
  let sms = newMessage(null, null, ICC_ID, "sent", "success", null, null, null,
                       "normal", new Date(), 0, true);
  do_check_true(sms instanceof Ci.nsIDOMMozSmsMessage);
  do_check_eq(sms.id, 0);
  do_check_eq(sms.threadId, 0);
  do_check_eq(sms.iccId, ICC_ID);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.receiver, null);
  do_check_eq(sms.sender, null);
  do_check_eq(sms.body, null);
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_true(sms.deliveryTimestamp === null);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test if ICC ID is null when it's not available.
 */
add_test(function test_icc_id_not_available() {
  let sms = newMessage(null, null, null, "sent", "success", null, null, null,
                       "normal", new Date(), 0, true);
  do_check_true(sms instanceof Ci.nsIDOMMozSmsMessage);
  do_check_eq(sms.id, 0);
  do_check_eq(sms.threadId, 0);
  do_check_eq(sms.iccId, null);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.receiver, null);
  do_check_eq(sms.sender, null);
  do_check_eq(sms.body, null);
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_true(sms.deliveryTimestamp === null);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Verify that attributes are read-only.
 */
add_test(function test_readonly_attributes() {
  let sms = newMessage(null, null, ICC_ID, "received", "success", null, null, null,
                       "normal", new Date(), 0, true);

  sms.id = 1;
  do_check_eq(sms.id, 0);

  sms.threadId = 1;
  do_check_eq(sms.threadId, 0);

  sms.iccId = "987654321";
  do_check_eq(sms.iccId, ICC_ID);

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

  let oldTimestamp = sms.timestamp;
  sms.timestamp = new Date();
  do_check_eq(sms.timestamp.getTime(), oldTimestamp.getTime());

  let oldDeliveryTimestamp = sms.deliveryTimestamp;
  sms.deliveryTimestamp = new Date();
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
                       "the body", "normal", ts, 0, true);
  do_check_eq(sms.id, 42);
  do_check_eq(sms.threadId, 1);
  do_check_eq(sms.iccId, ICC_ID);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.sender, "the sender");
  do_check_eq(sms.receiver, "the receiver");
  do_check_eq(sms.body, "the body");
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_eq(sms.timestamp.getTime(), ts);
  do_check_true(sms.deliveryTimestamp === null);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test supplying the timestamp as a Date object.
 */
add_test(function test_timestamp_date() {
  let date = new Date();
  let sms = newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
                       "the body", "normal", date, 0, true);
  do_check_eq(sms.id, 42);
  do_check_eq(sms.threadId, 1);
  do_check_eq(sms.iccId, ICC_ID);
  do_check_eq(sms.delivery, "sent");
  do_check_eq(sms.deliveryStatus, "success");
  do_check_eq(sms.sender, "the sender");
  do_check_eq(sms.receiver, "the receiver");
  do_check_eq(sms.body, "the body");
  do_check_eq(sms.messageClass, "normal");
  do_check_true(sms.timestamp instanceof Date);
  do_check_eq(sms.timestamp.getTime(), date.getTime());
  do_check_true(sms.deliveryTimestamp === null);
  do_check_true(sms.read);
  run_next_test();
});

/**
 * Test that a floating point number for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_float() {
  // Test timestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", 3.1415, 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  // Test deliveryTimestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", 0, 3.1415, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  run_next_test();
});

/**
 * Test that a null value for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_null() {
  // Test timestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", null, 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  // Test deliveryTimestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", 0, null, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  run_next_test();
});

/**
 * Test that undefined for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_undefined() {
  // Test timestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", undefined, 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  // Test deliveryTimestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", 0, undefined, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  run_next_test();
});

/**
 * Test that a random object for the timestamp is not allowed.
 */
add_test(function test_invalid_timestamp_object() {
  // Test timestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", {}, 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  // Test deliveryTimestamp.
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "normal", 0, {}, true);
  }, Cr.NS_ERROR_INVALID_ARG);

  run_next_test();
});

/**
 * Test that an invalid delivery string is not accepted.
 */
add_test(function test_invalid_delivery_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "this is invalid", "pending", "the sender",
               "the receiver", "the body", "normal", new Date(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'delivery' argument.
 */
add_test(function test_invalid_delivery_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, 1, "pending", "the sender", "the receiver", "the body",
               "normal", new Date(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid delivery status string is not accepted.
 */
add_test(function test_invalid_delivery_status_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "this is invalid", "the sender", "the receiver",
               "the body", "normal", new Date(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'deliveryStatus' argument.
 */
add_test(function test_invalid_delivery_status_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", 1, "the sender", "the receiver", "the body",
               "normal", new Date(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that an invalid message class string is not accepted.
 */
add_test(function test_invalid_message_class_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", "this is invalid", new Date(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});

/**
 * Test that a number is not accepted for the 'messageClass' argument.
 */
add_test(function test_invalid_message_class_string() {
  do_check_throws(function() {
    newMessage(42, 1, ICC_ID, "sent", "success", "the sender", "the receiver",
               "the body", 1, new Date(), 0, true);
  }, Cr.NS_ERROR_INVALID_ARG);
  run_next_test();
});
