/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

XPCOMUtils.defineLazyGetter(this, "gStkCmdFactory", function() {
  let stk = {};
  Cu.import("resource://gre/modules/StkProactiveCmdFactory.jsm", stk);
  return stk.StkProactiveCmdFactory;
});

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

  let smsMessenger_new = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                         .getService(Ci.nsISmsMessenger_new);

  let cellbroadcastMessenger = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                               .getService(Ci.nsICellbroadcastMessenger);

  let mobileConnectionMessenger = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                                  .getService(Ci.nsIMobileConnectionMessenger);

  let iccMessenger = Cc["@mozilla.org/ril/system-messenger-helper;1"]
                     .getService(Ci.nsIIccMessenger);

  ok(telephonyMessenger !== null, "Get TelephonyMessenger.");
  ok(smsMessenger != null, "Get SmsMessenger.");
  ok(smsMessenger_new != null, "Get SmsMessenger_new.");
  ok(cellbroadcastMessenger != null, "Get CellbroadcastMessenger.");
  ok(mobileConnectionMessenger != null, "Get MobileConnectionMessenger.");
  ok(iccMessenger != null, "Get IccMessenger.");

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

  // Verify 'sms-failed' system message.
  messenger.notifySms(Ci.nsISmsMessenger_new.NOTIFICATION_TYPE_SENT_FAILED,
                      7,
                      8,
                      "99887766554433221100",
                      Ci.nsISmsService.DELIVERY_TYPE_ERROR,
                      Ci.nsISmsService.DELIVERY_STATUS_TYPE_ERROR,
                      null,
                      "+0987654321",
                      "Outgoing message",
                      Ci.nsISmsService.MESSAGE_CLASS_TYPE_NORMAL,
                      timestamp,
                      0,
                      0,
                      true);

  equal_received_system_message("sms-failed", {
      iccId:             "99887766554433221100",
      type:              "sms",
      id:                7,
      threadId:          8,
      delivery:          "error",
      deliveryStatus:    "error",
      sender:            null,
      receiver:          "+0987654321",
      body:              "Outgoing message",
      messageClass:      "normal",
      timestamp:         timestamp,
      sentTimestamp:     0,
      deliveryTimestamp: 0,
      read:              true
    });

  // Verify 'sms-delivery-error' system message.
  messenger.notifySms(Ci.nsISmsMessenger_new.NOTIFICATION_TYPE_DELIVERY_ERROR,
                      9,
                      10,
                      "99887766554433221100",
                      Ci.nsISmsService.DELIVERY_TYPE_SENT,
                      Ci.nsISmsService.DELIVERY_STATUS_TYPE_ERROR,
                      null,
                      "+0987654321",
                      "Outgoing message",
                      Ci.nsISmsService.MESSAGE_CLASS_TYPE_NORMAL,
                      timestamp,
                      0,
                      0,
                      true);

  equal_received_system_message("sms-delivery-error", {
      iccId:             "99887766554433221100",
      type:              "sms",
      id:                9,
      threadId:          10,
      delivery:          "sent",
      deliveryStatus:    "error",
      sender:            null,
      receiver:          "+0987654321",
      body:              "Outgoing message",
      messageClass:      "normal",
      timestamp:         timestamp,
      sentTimestamp:     0,
      deliveryTimestamp: 0,
      read:              true
    });

  // Verify the protection of invalid nsISmsMessenger.NOTIFICATION_TYPEs.
  try {
    messenger.notifySms(5,
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

/**
 * Verify RILSystemMessenger.notifyUssdReceived()
 */
add_test(function test_mobileconnection_notify_ussd_received() {
  let messenger = newRILSystemMessenger();

  messenger.notifyUssdReceived(0, "USSD Message", false);

  equal_received_system_message("ussd-received", {
    serviceId: 0,
    message: "USSD Message",
    sessionEnded: false
  });

  messenger.notifyUssdReceived(1, "USSD Message", true);

  equal_received_system_message("ussd-received", {
    serviceId: 1,
    message: "USSD Message",
    sessionEnded: true
  });

  run_next_test();
});

/**
 * Verify RILSystemMessenger.notifyCdmaInfoRecXXX()
 */
add_test(function test_mobileconnection_notify_cdma_info() {
  let messenger = newRILSystemMessenger();

  messenger.notifyCdmaInfoRecDisplay(0, "CDMA Display Info");

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 0,
    display: "CDMA Display Info"
  });

  messenger.notifyCdmaInfoRecCalledPartyNumber(1, 1, 2, "+0987654321", 3, 4);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 1,
    calledNumber: {
      type: 1,
      plan: 2,
      number: "+0987654321",
      pi: 3,
      si: 4
    }
  });

  messenger.notifyCdmaInfoRecCallingPartyNumber(0, 5, 6, "+1234567890", 7, 8);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 0,
    callingNumber: {
      type: 5,
      plan: 6,
      number: "+1234567890",
      pi: 7,
      si: 8
    }
  });

  messenger.notifyCdmaInfoRecConnectedPartyNumber(1, 4, 3, "+56473839201", 2, 1);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 1,
    connectedNumber: {
      type: 4,
      plan: 3,
      number: "+56473839201",
      pi: 2,
      si: 1
    }
  });

  messenger.notifyCdmaInfoRecSignal(0, 1, 2, 3);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 0,
      signal: {
        type: 1,
        alertPitch: 2,
        signal: 3
      }
  });

  messenger.notifyCdmaInfoRecRedirectingNumber(1, 8, 7, "+1029384756", 6, 5, 4);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 1,
    redirect: {
      type: 8,
      plan: 7,
      number: "+1029384756",
      pi: 6,
      si: 5,
      reason: 4
    }
  });

  messenger.notifyCdmaInfoRecLineControl(0, 1, 0, 1, 255);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 0,
    lineControl: {
      polarityIncluded: 1,
      toggle: 0,
      reverse: 1,
      powerDenial: 255
    }
  });

  messenger.notifyCdmaInfoRecClir(1, 256);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 1,
    clirCause: 256
  });

  messenger.notifyCdmaInfoRecAudioControl(0, 255, -1);

  equal_received_system_message("cdma-info-rec-received", {
    clientId: 0,
    audioControl: {
      upLink: 255,
      downLink: -1
    }
  });

  run_next_test();
});

/**
 * Verify Error Handling of StkProactiveCmdFactory.createCommand()
 */
add_test(function test_icc_stk_cmd_factory_create_command_error() {
  let messenger = newRILSystemMessenger();

  // Verify the protection of invalid typeOfCommand.
  try {
    gStkCmdFactory.createCommand({
      commandNumber: 0,
      typeOfCommand: RIL.STK_CMD_MORE_TIME, // Invalid TypeOfCommand
      commandQualifier: 0x00
    });

    ok(false, "Failed to verify the protection of createCommand()!");
  } catch (e) {
    equal(e.message, "Unknown Command Type: " + RIL.STK_CMD_MORE_TIME);
  }

  run_next_test();
});

/**
 * Verify Error Handling of StkProactiveCmdFactory.createCommandMessage()
 */
add_test(function test_icc_stk_cmd_factory_create_system_msg_invalid_cmd_type() {
  let messenger = newRILSystemMessenger();
  let iccId = "99887766554433221100";

  // Verify the protection of invalid typeOfCommand.
  try {
    gStkCmdFactory.createCommandMessage({
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd]),

      // nsIStkProactiveCmd
      commandNumber: 0,
      typeOfCommand: RIL.STK_CMD_MORE_TIME, // Invalid TypeOfCommand
      commandQualifier: 0
    });

    ok(false, "Failed to identify invalid typeOfCommand!");
  } catch (e) {
    equal(e.message, "Unknown Command Type: " + RIL.STK_CMD_MORE_TIME);
  }

  run_next_test();
});

/**
 * Verify Error Handling of StkProactiveCmdFactory.createCommandMessage()
 */
add_test(function test_icc_stk_cmd_factory_create_system_msg_incorrect_cmd_type() {
  let messenger = newRILSystemMessenger();
  let iccId = "99887766554433221100";

  // Verify the protection of invalid typeOfCommand.
  try {
    gStkCmdFactory.createCommandMessage({
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                             Ci.nsIStkProvideLocalInfoCmd]),

      // nsIStkProactiveCmd
      commandNumber: 0,
      typeOfCommand: RIL.STK_CMD_POLL_INTERVAL, // Incorrect typeOfCommand
      commandQualifier: 0,
      // nsIStkProvideLocalInfoCmd
      localInfoType: 0x00,
    });

    ok(false, "Failed to identify incorrect typeOfCommand!");
  } catch (e) {
    ok(e.message.indexOf("Failed to convert command into concrete class: ") !== -1);
  }

  run_next_test();
});

/**
 * Verify RILSystemMessenger.notifyStkProactiveCommand()
 */
add_test(function test_icc_notify_stk_proactive_command() {
  let messenger = newRILSystemMessenger();
  let iccId = "99887766554433221100";
  let WHT = 0xFFFFFFFF;
  let BLK = 0x000000FF;
  let RED = 0xFF0000FF;
  let GRN = 0x00FF00FF;
  let BLU = 0x0000FFFF;
  let TSP = 0;
  // Basic Image, see Anex B.1 in TS 31.102.
  let basicIcon = {
    width: 8,
    height: 8,
    codingScheme: "basic",
    pixels: [WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,
             BLK, BLK, BLK, BLK, BLK, BLK, WHT, WHT,
             WHT, BLK, WHT, BLK, BLK, WHT, BLK, WHT,
             WHT, BLK, BLK, WHT, WHT, BLK, BLK, WHT,
             WHT, BLK, BLK, WHT, WHT, BLK, BLK, WHT,
             WHT, BLK, WHT, BLK, BLK, WHT, BLK, WHT,
             WHT, WHT, BLK, BLK, BLK, BLK, WHT, WHT,
             WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT]
  };
  // Color Image, see Anex B.2 in TS 31.102.
  let colorIcon = {
    width: 8,
    height: 8,
    codingScheme: "color",
    pixels: [BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU,
             BLU, RED, RED, RED, RED, RED, RED, BLU,
             BLU, RED, GRN, GRN, GRN, RED, RED, BLU,
             BLU, RED, RED, GRN, GRN, RED, RED, BLU,
             BLU, RED, RED, GRN, GRN, RED, RED, BLU,
             BLU, RED, RED, GRN, GRN, GRN, RED, BLU,
             BLU, RED, RED, RED, RED, RED, RED, BLU,
             BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU]
  };
  // Color Image with Transparency, see Anex B.2 in TS 31.102.
  let colorTransparencyIcon = {
    width: 8,
    height: 8,
    codingScheme: "color-transparency",
    pixels: [TSP, TSP, TSP, TSP, TSP, TSP, TSP, TSP,
             TSP, RED, RED, RED, RED, RED, RED, TSP,
             TSP, RED, GRN, GRN, GRN, RED, RED, TSP,
             TSP, RED, RED, GRN, GRN, RED, RED, TSP,
             TSP, RED, RED, GRN, GRN, RED, RED, TSP,
             TSP, RED, RED, GRN, GRN, GRN, RED, TSP,
             TSP, RED, RED, RED, RED, RED, RED, TSP,
             TSP, TSP, TSP, TSP, TSP, TSP, TSP, TSP]
  };

  let cmdCount = 0;

  // Test Messages:
  let messages = [
    // STK_CMD_REFRESH
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_REFRESH,
      commandQualifier: 0x04 // UICC Reset
    },
    // STK_CMD_POLL_INTERVAL
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_POLL_INTERVAL,
      commandQualifier: 0x00, // RFU
      options: {
        timeUnit: RIL.STK_TIME_UNIT_TENTH_SECOND,
        timeInterval: 0x05
      }
    },
    // STK_CMD_POLL_OFF
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_POLL_OFF,
      commandQualifier: 0x00, // RFU
    },
    // STK_CMD_PROVIDE_LOCAL_INFO
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_PROVIDE_LOCAL_INFO,
      commandQualifier: 0x01, // IMEI of the terminal
      options: {
        localInfoType: 0x01 // IMEI of the terminal
      }
    },
    // STK_CMD_SET_UP_EVENT_LIST with eventList
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_EVENT_LIST,
      commandQualifier: 0x00, // RFU
      options: {
        eventList: [ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                     0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                     0x18, 0x19, 0x1A, 0x1B, 0x1C ]
      }
    },
    // STK_CMD_SET_UP_EVENT_LIST without eventList
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_EVENT_LIST,
      commandQualifier: 0x00, // RFU
      options: {
        eventList: null
      }
    },
    // STK_CMD_SET_UP_MENU with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_MENU,
      commandQualifier: 0x80, // bit 8: 1 = help information available
      options: {
        title: "Toolkit Menu 1",
        items: [
          { identifier: 0x01, text: "Menu Item 1" },
          { identifier: 0x02, text: "Menu Item 2" },
          { identifier: 0x03, text: "Menu Item 3" }
        ],
        isHelpAvailable: true
      }
    },
    // STK_CMD_SET_UP_MENU with optional properties including:
    // iconInfo for this menu, iconInfo for each item and nextActionList.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_MENU,
      commandQualifier: 0x00, // bit 8: 0 = help information is not available
      options: {
        title: "Toolkit Menu 2",
        items: [
          { identifier: 0x01,
            text: "Menu Item 1",
            iconSelfExplanatory: true,
            icons: [basicIcon]
          },
          { identifier: 0x02,
            text: "Menu Item 2",
            iconSelfExplanatory: false,
            icons: [basicIcon, colorIcon]
          },
          { identifier: 0x03,
            text: "Menu Item 3",
            iconSelfExplanatory: true,
            icons: [basicIcon, colorIcon, colorTransparencyIcon]
          },
        ],
        nextActionList: [
          RIL.STK_NEXT_ACTION_END_PROACTIVE_SESSION,
          RIL.STK_NEXT_ACTION_NULL,
          RIL.STK_NEXT_ACTION_NULL,
          RIL.STK_NEXT_ACTION_NULL
        ],
        iconSelfExplanatory: false,
        icons: [basicIcon, colorIcon, colorTransparencyIcon],
        isHelpAvailable: false
      }
    },
    // STK_CMD_SELECT_ITEM with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SELECT_ITEM,
      commandQualifier: RIL.STK_PRESENTATION_TYPE_NOT_SPECIFIED,
      options: {
        title: null,
        items: [
          { identifier: 0x01, text: "Menu Item 1" },
          { identifier: 0x02, text: "Menu Item 2" },
          { identifier: 0x03, text: "Menu Item 3" }
        ],
        presentationType: RIL.STK_PRESENTATION_TYPE_NOT_SPECIFIED,
        isHelpAvailable: false
      }
    },
    // STK_CMD_SELECT_ITEM with optional properties including:
    // title, iconInfo for this menu, iconInfo for each item and nextActionList.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SELECT_ITEM,
      commandQualifier: RIL.STK_PRESENTATION_TYPE_NAVIGATION_OPTIONS,
      options: {
        title: "Selected Toolkit Menu",
        items: [
          { identifier: 0x01,
            text: "Menu Item 1",
            iconSelfExplanatory: true,
            icons: [basicIcon]
          },
          { identifier: 0x02,
            text: "Menu Item 2",
            iconSelfExplanatory: false,
            icons: [basicIcon, colorIcon]
          },
          { identifier: 0x03,
            text: "Menu Item 3",
            iconSelfExplanatory: true,
            icons: [basicIcon, colorIcon, colorTransparencyIcon]
          },
        ],
        nextActionList: [
          RIL.STK_NEXT_ACTION_END_PROACTIVE_SESSION,
          RIL.STK_NEXT_ACTION_NULL,
          RIL.STK_NEXT_ACTION_NULL,
          RIL.STK_NEXT_ACTION_NULL
        ],
        defaultItem: 0x02,
        iconSelfExplanatory: false,
        icons: [basicIcon, colorIcon, colorTransparencyIcon],
        presentationType: RIL.STK_PRESENTATION_TYPE_NAVIGATION_OPTIONS,
        isHelpAvailable: false
      }
    },
    // STK_CMD_DISPLAY_TEXT with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_DISPLAY_TEXT,
      commandQualifier: 0x01, // bit 1: High Priority
      options: {
        text: "Display Text 1",
        isHighPriority: true,
        userClear: false,
        responseNeeded: false
      }
    },
    // STK_CMD_DISPLAY_TEXT with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_DISPLAY_TEXT,
      commandQualifier: 0x80, // bit 8: User Clear
      options: {
        text: "Display Text 2",
        isHighPriority: false,
        userClear: true,
        responseNeeded: true,
        duration: {
          timeUnit: RIL.STK_TIME_UNIT_TENTH_SECOND,
          timeInterval: 0x05
        },
        iconSelfExplanatory: true,
        icons: [basicIcon]
      }
    },
    // STK_CMD_SET_UP_IDLE_MODE_TEXT
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_IDLE_MODE_TEXT,
      commandQualifier: 0x00, // RFU
      options: {
        text: "Setup Idle Mode Text"
      }
    },
    // STK_CMD_SEND_SS
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SEND_SS,
      commandQualifier: 0x00, // RFU
      options: {
        text: "Send SS",
        iconSelfExplanatory: true,
        icons: [colorIcon]
      }
    },
    // STK_CMD_SEND_USSD
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SEND_USSD,
      commandQualifier: 0x00, // RFU
      options: {
        text: "Send USSD"
      }
    },
    // STK_CMD_SEND_SMS
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SEND_SMS,
      commandQualifier: 0x00, // RFU
      options: {
        text: "Send SMS",
        iconSelfExplanatory: false,
        icons: [colorTransparencyIcon]
      }
    },
    // STK_CMD_SEND_DTMF
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SEND_DTMF,
      commandQualifier: 0x00, // RFU
      options: {
        text: "Send DTMF",
        iconSelfExplanatory: true,
        icons: [basicIcon]
      }
    },
    // STK_CMD_GET_INKEY
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_GET_INKEY,
      commandQualifier: 0x84, // bit 3: isYesNoRequested, bit 8: isHelpAvailable
      options: {
        text: "Get Input Key",
        minLength: 1,
        maxLength: 1,
        duration: {
          timeUnit: RIL.STK_TIME_UNIT_SECOND,
          timeInterval: 0x0A
        },
        isAlphabet: false,
        isUCS2: false,
        isYesNoRequested: true,
        isHelpAvailable: true,
        defaultText: null,
        iconSelfExplanatory: false,
        icons: [colorIcon]
      }
    },
    // STK_CMD_GET_INPUT
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_GET_INPUT,
      commandQualifier: 0x0F, // bit 1-4: isAlphabet, isUCS2, hideInput, isPacked
      options: {
        text: "Get Input Text",
        minLength: 1,
        maxLength: 255,
        defaultText: "Default Input Text",
        isAlphabet: true,
        isUCS2: true,
        hideInput: true,
        isPacked: true,
        isHelpAvailable: false,
        defaultText: null,
        iconSelfExplanatory: true,
        icons: [basicIcon]
      }
    },
    // STK_CMD_SET_UP_CALL with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_CALL,
      commandQualifier: 0x00, // RFU
      options: {
        address: "+0987654321"
      }
    },
    // STK_CMD_SET_UP_CALL with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SET_UP_CALL,
      commandQualifier: 0x00, // RFU
      options: {
        address: "+0987654321",
        confirmMessage: {
          text: "Confirm Message",
          iconSelfExplanatory: false,
          icons: [colorIcon]
        },
        callMessage: {
          text: "Call Message",
          iconSelfExplanatory: true,
          icons: [basicIcon]
        },
        duration: {
          timeUnit: RIL.STK_TIME_UNIT_SECOND,
          timeInterval: 0x0A
        }
      }
    },
    // STK_CMD_LAUNCH_BROWSER with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_LAUNCH_BROWSER,
      commandQualifier: RIL.STK_BROWSER_MODE_USING_NEW_BROWSER,
      options: {
        url: "http://www.mozilla.org",
        mode: RIL.STK_BROWSER_MODE_USING_NEW_BROWSER
      }
    },
    // STK_CMD_LAUNCH_BROWSER with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_LAUNCH_BROWSER,
      commandQualifier: RIL.STK_BROWSER_MODE_USING_NEW_BROWSER,
      options: {
        url: "http://www.mozilla.org",
        mode: RIL.STK_BROWSER_MODE_USING_NEW_BROWSER,
        confirmMessage: {
          text: "Confirm Message for Launch Browser",
          iconSelfExplanatory: false,
          icons: [colorTransparencyIcon]
        }
      }
    },
    // STK_CMD_PLAY_TONE with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_PLAY_TONE,
      commandQualifier: 0x01, // isVibrate
      options: {
        text: null,
        isVibrate: true
      }
    },
    // STK_CMD_PLAY_TONE with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_PLAY_TONE,
      commandQualifier: 0x00, // isVibrate = false
      options: {
        text: "Play Tone",
        tone: RIL.STK_TONE_TYPE_CONGESTION,
        isVibrate: false,
        duration: {
          timeUnit: RIL.STK_TIME_UNIT_SECOND,
          timeInterval: 0x0A
        },
        iconSelfExplanatory: true,
        icons: [basicIcon]
      }
    },
    // STK_CMD_TIMER_MANAGEMENT with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_TIMER_MANAGEMENT,
      commandQualifier: RIL.STK_TIMER_DEACTIVATE,
      options: {
        timerId: 0x08,
        timerAction: RIL.STK_TIMER_DEACTIVATE
      }
    },
    // STK_CMD_TIMER_MANAGEMENT with optional properties.
    {
      commandNumber: ++cmdCount,
        typeOfCommand: RIL.STK_CMD_TIMER_MANAGEMENT,
        commandQualifier: RIL.STK_TIMER_START,
        options: {
          timerId: 0x01,
          timerValue: (12 * 60 * 60) + (30 * 60) + (30), // 12:30:30
          timerAction: RIL.STK_TIMER_START
        }
    },
    // STK_CMD_OPEN_CHANNEL with mandatory properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_OPEN_CHANNEL,
      commandQualifier: 0x00,  //RFU
      options: {
        text: null,
      }
    },
    // STK_CMD_OPEN_CHANNEL with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_OPEN_CHANNEL,
      commandQualifier: 0x00,  //RFU
      options: {
        text: "Open Channel",
        iconSelfExplanatory: false,
        icons: [colorIcon]
      }
    },
    // STK_CMD_CLOSE_CHANNEL with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_CLOSE_CHANNEL,
      commandQualifier: 0x00,  //RFU
      options: {
        text: "Close Channel",
        iconSelfExplanatory: true,
        icons: [colorTransparencyIcon]
      }
    },
    // STK_CMD_SEND_DATA with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_SEND_DATA,
      commandQualifier: 0x00,  //RFU
      options: {
        text: null,
        iconSelfExplanatory: false,
        icons: [basicIcon]
      }
    },
    // STK_CMD_RECEIVE_DATA with optional properties.
    {
      commandNumber: ++cmdCount,
      typeOfCommand: RIL.STK_CMD_RECEIVE_DATA,
      commandQualifier: 0x00,  //RFU
      options: {
        text: "Receive Data"
      }
    },
    null // Termination condition to run_next_test()
  ];

  messages.forEach(function(aMessage) {
    if (!aMessage) {
      run_next_test();
      return;
    }

    messenger.notifyStkProactiveCommand(iccId,
                                        gStkCmdFactory.createCommand(aMessage));

    equal_received_system_message("icc-stkcommand", {
      iccId: iccId,
      command: aMessage
    });
  });
});
