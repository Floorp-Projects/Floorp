/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Name space for RILSystemMessenger.jsm. Only initialized after first call to
 * newRILSystemMessenger.
 */
let RSM;

let gReceivedMsgType = null;
let gReceivedMessage = null;

/**
 * Create a new RILSystemMessenger instance.
 *
 * @return a RILSystemMessenger instance.
 */
function newRILSystemMessenger() {
  if (!RSM) {
    RSM = Cu.import("resource://gre/modules/RILSystemMessenger.jsm", {});
    equal(typeof RSM.RILSystemMessenger, "function", "RSM.RILSystemMessenger");
  }

  let rsm = new RSM.RILSystemMessenger();
  rsm.broadcastMessage = (aType, aMessage) => {
    gReceivedMsgType = aType;
    gReceivedMessage = aMessage;
  };

  return rsm;
}

function equal_received_system_message(aType, aMessage) {
  equal(aType, gReceivedMsgType);
  deepEqual(aMessage, gReceivedMessage);
  gReceivedMsgType = null;
  gReceivedMessage = null;
}

/**
 * Verify that each nsIXxxMessenger could be retrieved.
 */
function run_test() {
  let telephonyMessenger = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                           .getService(Ci.nsITelephonyMessenger);

  let smsMessenger = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                     .getService(Ci.nsISmsMessenger);

  let cellbroadcastMessenger = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                               .getService(Ci.nsICellbroadcastMessenger);

  ok(telephonyMessenger !== null, "Get TelephonyMessenger.");
  ok(smsMessenger != null, "Get SmsMessenger.");
  ok(cellbroadcastMessenger != null, "Get CellbroadcastMessenger.");

  run_next_test();
}

/**
 * Verify RILSystemMessenger.notifyNewCall()
 */
add_test(function test_telephony_messenger_notify_new_call() {
  let messenger = newRILSystemMessenger();

  messenger.notifyNewCall();
  equal_received_system_message("telephony-new-call", {});

  run_next_test();
});

/**
 * Verify RILSystemMessenger.notifyCallEnded()
 */
add_test(function test_telephony_messenger_notify_call_ended() {
  let messenger = newRILSystemMessenger();

  messenger.notifyCallEnded(1,
                            "+0987654321",
                            null,
                            true,
                            500,
                            false,
                            true);

  equal_received_system_message("telephony-call-ended", {
    serviceId: 1,
    number: "+0987654321",
    emergency: true,
    duration: 500,
    direction: "incoming",
    hangUpLocal: true
  });

  // Verify 'optional' parameter of secondNumber.
  messenger.notifyCallEnded(1,
                            "+0987654321",
                            "+1234567890",
                            true,
                            500,
                            true,
                            false);

  equal_received_system_message("telephony-call-ended", {
    serviceId: 1,
    number: "+0987654321",
    emergency: true,
    duration: 500,
    direction: "outgoing",
    hangUpLocal: false,
    secondNumber: "+1234567890"
  });

  run_next_test();
});

/**
 * Verify RILSystemMessenger.notifySms()
 */
add_test(function test_sms_messenger_notify_sms() {
  let messenger = newRILSystemMessenger();
  let timestamp = Date.now();
  let sentTimestamp = timestamp + 100;
  let deliveryTimestamp = sentTimestamp + 100;

  // Verify 'sms-received' system message.
  messenger.notifySms(Ci.nsISmsMessenger.NOTIFICATION_TYPE_RECEIVED,
                      1,
                      2,
                      "99887766554433221100",
                      Ci.nsISmsService.DELIVERY_TYPE_RECEIVED,
                      Ci.nsISmsService.DELIVERY_STATUS_TYPE_SUCCESS,
                      "+0987654321",
                      null,
                      "Incoming message",
                      Ci.nsISmsService.MESSAGE_CLASS_TYPE_CLASS_2,
                      timestamp,
                      sentTimestamp,
                      0,
                      false);

  equal_received_system_message("sms-received", {
      iccId:             "99887766554433221100",
      type:              "sms",
      id:                1,
      threadId:          2,
      delivery:          "received",
      deliveryStatus:    "success",
      sender:            "+0987654321",
      receiver:          null,
      body:              "Incoming message",
      messageClass:      "class-2",
      timestamp:         timestamp,
      sentTimestamp:     sentTimestamp,
      deliveryTimestamp: 0,
      read:              false
    });

  // Verify 'sms-sent' system message.
  messenger.notifySms(Ci.nsISmsMessenger.NOTIFICATION_TYPE_SENT,
                      3,
                      4,
                      "99887766554433221100",
                      Ci.nsISmsService.DELIVERY_TYPE_SENT,
                      Ci.nsISmsService.DELIVERY_STATUS_TYPE_PENDING,
                      null,
                      "+0987654321",
                      "Outgoing message",
                      Ci.nsISmsService.MESSAGE_CLASS_TYPE_NORMAL,
                      timestamp,
                      0,
                      0,
                      true);

  equal_received_system_message("sms-sent", {
      iccId:             "99887766554433221100",
      type:              "sms",
      id:                3,
      threadId:          4,
      delivery:          "sent",
      deliveryStatus:    "pending",
      sender:            null,
      receiver:          "+0987654321",
      body:              "Outgoing message",
      messageClass:      "normal",
      timestamp:         timestamp,
      sentTimestamp:     0,
      deliveryTimestamp: 0,
      read:              true
    });

  // Verify 'sms-delivery-success' system message.
  messenger.notifySms(Ci.nsISmsMessenger.NOTIFICATION_TYPE_DELIVERY_SUCCESS,
                      5,
                      6,
                      "99887766554433221100",
                      Ci.nsISmsService.DELIVERY_TYPE_SENT,
                      Ci.nsISmsService.DELIVERY_STATUS_TYPE_SUCCESS,
                      null,
                      "+0987654321",
                      "Outgoing message",
                      Ci.nsISmsService.MESSAGE_CLASS_TYPE_NORMAL,
                      timestamp,
                      0,
                      deliveryTimestamp,
                      true);

  equal_received_system_message("sms-delivery-success", {
      iccId:             "99887766554433221100",
      type:              "sms",
      id:                5,
      threadId:          6,
      delivery:          "sent",
      deliveryStatus:    "success",
      sender:            null,
      receiver:          "+0987654321",
      body:              "Outgoing message",
      messageClass:      "normal",
      timestamp:         timestamp,
      sentTimestamp:     0,
      deliveryTimestamp: deliveryTimestamp,
      read:              true
    });

  // Verify the protection of invalid nsISmsMessenger.NOTIFICATION_TYPEs.
  try {
    messenger.notifySms(3,
                        1,
                        2,
                        "99887766554433221100",
                        Ci.nsISmsService.DELIVERY_TYPE_RECEIVED,
                        Ci.nsISmsService.DELIVERY_STATUS_TYPE_SUCCESS,
                        "+0987654321",
                        null,
                        "Incoming message",
                        Ci.nsISmsService.MESSAGE_CLASS_TYPE_NORMAL,
                        timestamp,
                        sentTimestamp,
                        0,
                        false);
    ok(false, "Failed to verify the protection of invalid nsISmsMessenger.NOTIFICATION_TYPE!");
  } catch (e) {}

  run_next_test();
});

/**
 * Verify RILSystemMessenger.notifyCbMessageReceived()
 */
add_test(function test_cellbroadcast_messenger_notify_cb_message_received() {
  let messenger = newRILSystemMessenger();
  let timestamp = Date.now();

  // Verify ETWS
  messenger.notifyCbMessageReceived(0,
                                    Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_CELL_IMMEDIATE,
                                    256,
                                    4352,
                                    null,
                                    null,
                                    Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_NORMAL,
                                    timestamp,
                                    Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID,
                                    true,
                                    Ci.nsICellBroadcastService.GSM_ETWS_WARNING_EARTHQUAKE,
                                    false,
                                    true);
  equal_received_system_message("cellbroadcast-received", {
      serviceId: 0,
      gsmGeographicalScope: "cell-immediate",
      messageCode: 256,
      messageId: 4352,
      language: null,
      body: null,
      messageClass: "normal",
      timestamp: timestamp,
      cdmaServiceCategory: null,
      etws: {
        warningType: "earthquake",
        emergencyUserAlert: false,
        popup: true
      }
  });

  // Verify Normal CB Message
  messenger.notifyCbMessageReceived(1,
                                    Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_PLMN,
                                    0,
                                    50,
                                    "en",
                                    "The quick brown fox jumps over the lazy dog",
                                    Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_NORMAL,
                                    timestamp,
                                    Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID,
                                    false,
                                    Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID,
                                    false,
                                    false);
  equal_received_system_message("cellbroadcast-received", {
      serviceId: 1,
      gsmGeographicalScope: "plmn",
      messageCode: 0,
      messageId: 50,
      language: "en",
      body: "The quick brown fox jumps over the lazy dog",
      messageClass: "normal",
      timestamp: timestamp,
      cdmaServiceCategory: null,
      etws: null
  });

  // Verify CB Message with ETWS Info
  messenger.notifyCbMessageReceived(0,
                                    Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_LOCATION_AREA,
                                    0,
                                    4354,
                                    "en",
                                    "Earthquake & Tsunami Warning!",
                                    Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_0,
                                    timestamp,
                                    Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID,
                                    true,
                                    Ci.nsICellBroadcastService.GSM_ETWS_WARNING_EARTHQUAKE_TSUNAMI,
                                    true,
                                    false);
  equal_received_system_message("cellbroadcast-received", {
      serviceId: 0,
      gsmGeographicalScope: "location-area",
      messageCode: 0,
      messageId: 4354,
      language: "en",
      body: "Earthquake & Tsunami Warning!",
      messageClass: "class-0",
      timestamp: timestamp,
      cdmaServiceCategory: null,
      etws: {
        warningType: "earthquake-tsunami",
        emergencyUserAlert: true,
        popup: false
      }
  });

  // Verify CDMA CB Message
  messenger.notifyCbMessageReceived(0,
                                    Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_INVALID,
                                    0,
                                    0,
                                    null,
                                    "CDMA CB Message",
                                    Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_NORMAL,
                                    timestamp,
                                    512,
                                    false,
                                    Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID,
                                    false,
                                    false);
  equal_received_system_message("cellbroadcast-received", {
      serviceId: 0,
      gsmGeographicalScope: null,
      messageCode: 0,
      messageId: 0,
      language: null,
      body: "CDMA CB Message",
      messageClass: "normal",
      timestamp: timestamp,
      cdmaServiceCategory: 512,
      etws: null
  });

  run_next_test();
});
