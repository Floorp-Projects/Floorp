/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Helper function.
 */
function newUint8SupportOutgoingIndexWorker() {
  let worker = newWorker();
  let index = 4;          // index for read
  let buf = [0, 0, 0, 0]; // Preserved parcel size
  let context = worker.ContextPool._contexts[0];

  context.Buf.writeUint8 = function(value) {
    if (context.Buf.outgoingIndex >= buf.length) {
      buf.push(value);
    } else {
      buf[context.Buf.outgoingIndex] = value;
    }

    context.Buf.outgoingIndex++;
  };

  context.Buf.readUint8 = function() {
    return buf[index++];
  };

  context.Buf.seekIncoming = function(offset) {
    index += offset;
  };

  worker.debug = do_print;

  return worker;
}

// Test RIL requests related to STK.
/**
 * Verify if RIL.sendStkTerminalProfile be called.
 */
add_test(function test_if_send_stk_terminal_profile() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let profileSend = false;
  context.RIL.sendStkTerminalProfile = function(data) {
    profileSend = true;
  };

  let iccStatus = {
    gsmUmtsSubscriptionAppIndex: 0,
    apps: [{
      app_state: CARD_APPSTATE_READY,
      app_type: CARD_APPTYPE_USIM
    }],
  };
  worker.RILQUIRKS_SEND_STK_PROFILE_DOWNLOAD = false;

  context.RIL._processICCStatus(iccStatus);

  equal(profileSend, false);

  run_next_test();
});

/**
 * Verify RIL.sendStkTerminalProfile
 */
add_test(function test_send_stk_terminal_profile() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;

  ril.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);

  buf.seekIncoming(8);
  let profile = buf.readString();
  for (let i = 0; i < STK_SUPPORTED_TERMINAL_PROFILE.length; i++) {
    equal(parseInt(profile.substring(2 * i, 2 * i + 2), 16),
                STK_SUPPORTED_TERMINAL_PROFILE[i]);
  }

  run_next_test();
});

/**
 * Verify STK terminal response
 */
add_test(function test_stk_terminal_response() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // Token : we don't care
    this.readInt32();

    // Data Size, 44 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
    //                      TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_RESULT_SIZE(3) +
    //                      TEXT LENGTH(10))
    equal(this.readInt32(), 44);

    // Command Details, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 3);
    equal(pduHelper.readHexOctet(), 0x01);
    equal(pduHelper.readHexOctet(), STK_CMD_PROVIDE_LOCAL_INFO);
    equal(pduHelper.readHexOctet(), STK_LOCAL_INFO_NNA);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Result
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_RESULT_OK);

    // Text
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_TEXT_STRING |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 8);
    equal(pduHelper.readHexOctet(), STK_TEXT_CODING_GSM_7BIT_PACKED);
    equal(pduHelper.readSeptetsToString(7, 0, PDU_NL_IDENTIFIER_DEFAULT,
                PDU_NL_IDENTIFIER_DEFAULT), "Mozilla");

    run_next_test();
  };

  let response = {
    command: {
      commandNumber: 0x01,
      typeOfCommand: STK_CMD_PROVIDE_LOCAL_INFO,
      commandQualifier: STK_LOCAL_INFO_NNA,
      options: {
        isPacked: true
      }
    },
    input: "Mozilla",
    resultCode: STK_RESULT_OK
  };
  context.RIL.sendStkTerminalResponse(response);
});

/**
 * Verify STK terminal response : GET INPUT with empty string.
 *
 * @See |TERMINAL RESPONSE: GET INPUT 1.9.1A| of 27.22.4.3.1 GET INPUT (normal)
 *      in TS 102 384.
 */
add_test(function test_stk_terminal_response_get_input_empty_string() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // Token : we don't care
    this.readInt32();

    // Data Size, 30 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
    //                      TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_RESULT_SIZE(3) +
    //                      TEXT LENGTH(3))
    equal(this.readInt32(), 30);

    // Command Details, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 3);
    equal(pduHelper.readHexOctet(), 0x01);
    equal(pduHelper.readHexOctet(), STK_CMD_GET_INPUT);
    equal(pduHelper.readHexOctet(), 0x00);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Result
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_RESULT_OK);

    // Text
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_TEXT_STRING |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_TEXT_CODING_GSM_8BIT);

    run_next_test();
  };

  let response = {
    command: {
      commandNumber: 0x01,
      typeOfCommand: STK_CMD_GET_INPUT,
      commandQualifier: 0x00,
      options: {
        minLength: 0,
        maxLength: 1,
        defaultText: "<SEND>"
      }
    },
    input: "",
    resultCode: STK_RESULT_OK
  };
  context.RIL.sendStkTerminalResponse(response);
});

/**
 * Verify STK terminal response : GET INPUT with 160 unpacked characters.
 *
 * @See |TERMINAL RESPONSE: GET INPUT 1.8.1| of 27.22.4.3.1 GET INPUT (normal)
 *      in TS 102 384.
 */
add_test(function test_stk_terminal_response_get_input_160_unpacked_characters() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;
  let iccPduHelper = context.ICCPDUHelper;
  let TEST_TEXT_STRING = "***1111111111###" +
                         "***2222222222###" +
                         "***3333333333###" +
                         "***4444444444###" +
                         "***5555555555###" +
                         "***6666666666###" +
                         "***7777777777###" +
                         "***8888888888###" +
                         "***9999999999###" +
                         "***0000000000###";

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // Token : we don't care
    this.readInt32();

    // Data Size, 352 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
    //                       TLV_DEVICE_ID_SIZE(4) +
    //                       TLV_RESULT_SIZE(3) +
    //                       TEXT LENGTH(164))
    equal(this.readInt32(), 352);

    // Command Details, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 3);
    equal(pduHelper.readHexOctet(), 0x01);
    equal(pduHelper.readHexOctet(), STK_CMD_GET_INPUT);
    equal(pduHelper.readHexOctet(), 0x00);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Result
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_RESULT_OK);

    // Text
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_TEXT_STRING |
                                          COMPREHENSIONTLV_FLAG_CR);
    // C-TLV Length Encoding: 161 = 0x81 0xA1
    equal(pduHelper.readHexOctet(), 0x81);
    equal(pduHelper.readHexOctet(), 0xA1);
    equal(pduHelper.readHexOctet(), STK_TEXT_CODING_GSM_8BIT);
    equal(iccPduHelper.read8BitUnpackedToString(160), TEST_TEXT_STRING);

    run_next_test();
  };

  let response = {
    command: {
      commandNumber: 0x01,
      typeOfCommand: STK_CMD_GET_INPUT,
      commandQualifier: 0x00,
      options: {
        minLength: 160,
        maxLength: 160,
        text: TEST_TEXT_STRING
      }
    },
    input: TEST_TEXT_STRING,
    resultCode: STK_RESULT_OK
  };
  context.RIL.sendStkTerminalResponse(response);
});

/**
 * Verify STK terminal response : GET_INKEY - NO_RESPONSE_FROM_USER with
 * duration provided.
 *
 * @See |27.22.4.2.8 GET INKEY (Variable Time out)| in TS 102 384.
 */
add_test(function test_stk_terminal_response_get_inkey_no_response_from_user() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // Token : we don't care
    this.readInt32();

    // Data Size, 32 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
    //                       TLV_DEVICE_ID_SIZE(4) +
    //                       TLV_RESULT_SIZE(3) +
    //                       DURATION(4))
    equal(this.readInt32(), 32);

    // Command Details, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 3);
    equal(pduHelper.readHexOctet(), 0x01);
    equal(pduHelper.readHexOctet(), STK_CMD_GET_INKEY);
    equal(pduHelper.readHexOctet(), 0x00);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Result
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_RESULT_NO_RESPONSE_FROM_USER);

    // Duration, Type-Length-Value(Time unit, Time interval)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DURATION);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_TIME_UNIT_SECOND);
    equal(pduHelper.readHexOctet(), 10);

    run_next_test();
  };

  let response = {
    command: {
      commandNumber: 0x01,
      typeOfCommand: STK_CMD_GET_INKEY,
      commandQualifier: 0x00,
      options: {
        duration: {
          timeUnit: STK_TIME_UNIT_SECOND,
          timeInterval: 10
        },
        text: 'Enter "+"'
      }
    },
    resultCode: STK_RESULT_NO_RESPONSE_FROM_USER
  };
  context.RIL.sendStkTerminalResponse(response);
});

/**
 * Verify STK terminal response : GET_INKEY - YES/NO request
 */
add_test(function test_stk_terminal_response_get_inkey() {
  function do_test(isYesNo) {
    let worker = newUint8SupportOutgoingIndexWorker();
    let context = worker.ContextPool._contexts[0];
    let buf = context.Buf;
    let pduHelper = context.GsmPDUHelper;

    buf.sendParcel = function() {
      // Type
      equal(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

      // Token : we don't care
      this.readInt32();

      // Data Size, 32 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
      //                      TLV_DEVICE_ID_SIZE(4) +
      //                      TLV_RESULT_SIZE(3) +
      //                      TEXT LENGTH(4))
      equal(this.readInt32(), 32);

      // Command Details, Type-Length-Value
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                            COMPREHENSIONTLV_FLAG_CR);
      equal(pduHelper.readHexOctet(), 3);
      equal(pduHelper.readHexOctet(), 0x01);
      equal(pduHelper.readHexOctet(), STK_CMD_GET_INKEY);
      equal(pduHelper.readHexOctet(), 0x04);

      // Device Identifies, Type-Length-Value(Source ID-Destination ID)
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
      equal(pduHelper.readHexOctet(), 2);
      equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
      equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

      // Result
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                            COMPREHENSIONTLV_FLAG_CR);
      equal(pduHelper.readHexOctet(), 1);
      equal(pduHelper.readHexOctet(), STK_RESULT_OK);

      // Yes/No response
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_TEXT_STRING |
                                            COMPREHENSIONTLV_FLAG_CR);
      equal(pduHelper.readHexOctet(), 2);
      equal(pduHelper.readHexOctet(), STK_TEXT_CODING_GSM_8BIT);
      equal(pduHelper.readHexOctet(), isYesNo ? 0x01 : 0x00);
    };

    let response = {
      command: {
        commandNumber: 0x01,
        typeOfCommand: STK_CMD_GET_INKEY,
        commandQualifier: 0x04,
        options: {
          isYesNoRequested: true
        }
      },
      isYesNo: isYesNo,
      resultCode: STK_RESULT_OK
    };

    context.RIL.sendStkTerminalResponse(response);
  };

  // Test "Yes" response
  do_test(true);
  // Test "No" response
  do_test(false);

  run_next_test();
});

/**
 * Verify STK terminal response with additional information.
 */
add_test(function test_stk_terminal_response_with_additional_info() {
  function do_test(aInfo) {
    let worker = newUint8SupportOutgoingIndexWorker();
    let context = worker.ContextPool._contexts[0];
    let buf = context.Buf;
    let pduHelper = context.GsmPDUHelper;

    buf.sendParcel = function() {
      // Type
      equal(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

      // Token : we don't care
      this.readInt32();

      // Data Length 26 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
      //                       TLV_DEVICE_ID_SIZE(4) +
      //                       TLV_RESULT_SIZE(4))
      equal(this.readInt32(), 26);

      // Command Details, Type-Length-Value(commandNumber, typeOfCommand, commandQualifier)
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                            COMPREHENSIONTLV_FLAG_CR);
      equal(pduHelper.readHexOctet(), 3);
      equal(pduHelper.readHexOctet(), 0x01);
      equal(pduHelper.readHexOctet(), STK_CMD_DISPLAY_TEXT);
      equal(pduHelper.readHexOctet(), 0x01);

      // Device Identifies, Type-Length-Value(Source ID-Destination ID)
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
      equal(pduHelper.readHexOctet(), 2);
      equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
      equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

      // Result, Type-Length-Value(General result, Additional information on result)
      equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                            COMPREHENSIONTLV_FLAG_CR);
      equal(pduHelper.readHexOctet(), 2);
      equal(pduHelper.readHexOctet(), STK_RESULT_TERMINAL_CRNTLY_UNABLE_TO_PROCESS);
      equal(pduHelper.readHexOctet(), aInfo);
    };

    let response = {
      command: {
        commandNumber: 0x01,
        typeOfCommand: STK_CMD_DISPLAY_TEXT,
        commandQualifier: 0x01,
        options: {
          isHighPriority: true
        }
      },
      resultCode: STK_RESULT_TERMINAL_CRNTLY_UNABLE_TO_PROCESS,
      additionalInformation: aInfo
    };

    context.RIL.sendStkTerminalResponse(response);
  };

  do_test(0x01); // 'Screen is busy'

  run_next_test();
});

// Test ComprehensionTlvHelper

/**
 * Verify ComprehensionTlvHelper.writeLocationInfoTlv
 */
add_test(function test_write_location_info_tlv() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let tlvHelper = context.ComprehensionTlvHelper;

  // Test with 2-digit mnc, and gsmCellId obtained from UMTS network.
  let loc = {
    mcc: "466",
    mnc: "92",
    gsmLocationAreaCode : 10291,
    gsmCellId: 19072823
  };
  tlvHelper.writeLocationInfoTlv(loc);

  let tag = pduHelper.readHexOctet();
  equal(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  let length = pduHelper.readHexOctet();
  equal(length, 9);

  let mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  equal(mcc_mnc, "46692");

  let lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  equal(lac, 10291);

  let cellId = (pduHelper.readHexOctet() << 24) |
               (pduHelper.readHexOctet() << 16) |
               (pduHelper.readHexOctet() << 8)  |
               (pduHelper.readHexOctet());
  equal(cellId, 19072823);

  // Test with 1-digit mnc, and gsmCellId obtained from GSM network.
  loc = {
    mcc: "466",
    mnc: "02",
    gsmLocationAreaCode : 10291,
    gsmCellId: 65534
  };
  tlvHelper.writeLocationInfoTlv(loc);

  tag = pduHelper.readHexOctet();
  equal(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  length = pduHelper.readHexOctet();
  equal(length, 7);

  mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  equal(mcc_mnc, "46602");

  lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  equal(lac, 10291);

  cellId = (pduHelper.readHexOctet() << 8) | (pduHelper.readHexOctet());
  equal(cellId, 65534);

  // Test with 3-digit mnc, and gsmCellId obtained from GSM network.
  loc = {
    mcc: "466",
    mnc: "222",
    gsmLocationAreaCode : 10291,
    gsmCellId: 65534
  };
  tlvHelper.writeLocationInfoTlv(loc);

  tag = pduHelper.readHexOctet();
  equal(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  length = pduHelper.readHexOctet();
  equal(length, 7);

  mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  equal(mcc_mnc, "466222");

  lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  equal(lac, 10291);

  cellId = (pduHelper.readHexOctet() << 8) | (pduHelper.readHexOctet());
  equal(cellId, 65534);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeErrorNumber
 */
add_test(function test_write_disconnecting_cause() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let tlvHelper = context.ComprehensionTlvHelper;

  tlvHelper.writeCauseTlv(RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[CALL_FAIL_BUSY]);
  let tag = pduHelper.readHexOctet();
  equal(tag, COMPREHENSIONTLV_TAG_CAUSE | COMPREHENSIONTLV_FLAG_CR);
  let len = pduHelper.readHexOctet();
  equal(len, 2);  // We have one cause.
  let standard = pduHelper.readHexOctet();
  equal(standard, 0x60);
  let cause = pduHelper.readHexOctet();
  equal(cause, 0x80 | CALL_FAIL_BUSY);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.getSizeOfLengthOctets
 */
add_test(function test_get_size_of_length_octets() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let tlvHelper = context.ComprehensionTlvHelper;

  let length = 0x70;
  equal(tlvHelper.getSizeOfLengthOctets(length), 1);

  length = 0x80;
  equal(tlvHelper.getSizeOfLengthOctets(length), 2);

  length = 0x180;
  equal(tlvHelper.getSizeOfLengthOctets(length), 3);

  length = 0x18000;
  equal(tlvHelper.getSizeOfLengthOctets(length), 4);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeLength
 */
add_test(function test_write_length() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let tlvHelper = context.ComprehensionTlvHelper;

  let length = 0x70;
  tlvHelper.writeLength(length);
  equal(pduHelper.readHexOctet(), length);

  length = 0x80;
  tlvHelper.writeLength(length);
  equal(pduHelper.readHexOctet(), 0x81);
  equal(pduHelper.readHexOctet(), length);

  length = 0x180;
  tlvHelper.writeLength(length);
  equal(pduHelper.readHexOctet(), 0x82);
  equal(pduHelper.readHexOctet(), (length >> 8) & 0xff);
  equal(pduHelper.readHexOctet(), length & 0xff);

  length = 0x18000;
  tlvHelper.writeLength(length);
  equal(pduHelper.readHexOctet(), 0x83);
  equal(pduHelper.readHexOctet(), (length >> 16) & 0xff);
  equal(pduHelper.readHexOctet(), (length >> 8) & 0xff);
  equal(pduHelper.readHexOctet(), length & 0xff);

  run_next_test();
});

// Test Proactive commands.

function test_stk_proactive_command(aOptions) {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let stkFactory = context.StkCommandParamsFactory;

  let testPdu = aOptions.pdu;
  let testTypeOfCommand = aOptions.typeOfCommand;
  let testIcons = aOptions.icons;
  let testFunc = aOptions.testFunc;

  if (testIcons) {
    let ril = context.RIL;
    ril.iccInfoPrivate.sst = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x30]; //IMG: 39
    ril.appType = CARD_APPTYPE_SIM;

    // skip asynchornous process in IconLoader.loadIcons().
    let iconLoader = context.IconLoader;
    iconLoader.loadIcons = (recordNumbers, onsuccess, onerror) => {
      onsuccess(testIcons);
    };
  }

  for(let i = 0 ; i < testPdu.length; i++) {
    pduHelper.writeHexOctet(testPdu[i]);
  }

  let berTlv = berHelper.decode(testPdu.length);
  let ctlvs = berTlv.value;
  let ctlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  let cmdDetails = ctlv.value;
  equal(cmdDetails.typeOfCommand, testTypeOfCommand);

  stkFactory.createParam(cmdDetails, ctlvs, (aResult) => {
    cmdDetails.options = aResult;
    testFunc(context, cmdDetails, ctlvs);
  });
}

/**
 * Verify Proactive command helper : searchForSelectedTags
 */
add_test(function test_stk_proactive_command_search_for_selected_tags() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let tag_test = [
    0xD0,
    0x3C,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x31,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x32,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x33,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x34,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x35];

  for (let i = 0; i < tag_test.length; i++) {
    pduHelper.writeHexOctet(tag_test[i]);
  }

  let berTlv = berHelper.decode(tag_test.length);
  let selectedCtlvs =
      stkHelper.searchForSelectedTags(berTlv.value, [COMPREHENSIONTLV_TAG_ALPHA_ID]);
  let tlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
  equal(tlv.value.identifier, "alpha id 1");

  tlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
  equal(tlv.value.identifier, "alpha id 2");

  tlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
  equal(tlv.value.identifier, "alpha id 3");

  tlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
  equal(tlv.value.identifier, "alpha id 4");

  tlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
  equal(tlv.value.identifier, "alpha id 5");

  run_next_test();
});

/**
 * Verify Proactive Command : Refresh
 */
add_test(function test_stk_proactive_command_refresh() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x10,
      0x81, 0x03, 0x01, 0x01, 0x01,
      0x82, 0x02, 0x81, 0x82,
      0x92, 0x05, 0x01, 0x3F, 0x00, 0x2F, 0xE2
    ],
    typeOfCommand: STK_CMD_REFRESH,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let stkHelper = context.StkProactiveCmdHelper;
      let ctlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_FILE_LIST, ctlvs);
      equal(ctlv.value.fileList, "3F002FE2");
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Play Tone
 */
add_test(function test_stk_proactive_command_play_tone() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x1F,
      0x81, 0x03, 0x01, 0x20, 0x00,
      0x82, 0x02, 0x81, 0x03,
      0x85, 0x09, 0x44, 0x69, 0x61, 0x6C, 0x20, 0x54, 0x6F, 0x6E, 0x65,
      0x8E, 0x01, 0x01,
      0x84, 0x02, 0x01, 0x05,
      0x9E, 0x02, 0x00, 0x01
    ],
    typeOfCommand: STK_CMD_PLAY_TONE,
    icons: [1],
    testFunc: (context, cmdDetails, ctlvs) => {
      let playTone = cmdDetails.options;

      equal(playTone.text, "Dial Tone");
      equal(playTone.tone, STK_TONE_TYPE_DIAL_TONE);
      equal(playTone.duration.timeUnit, STK_TIME_UNIT_SECOND);
      equal(playTone.duration.timeInterval, 5);
      equal(playTone.iconSelfExplanatory, true);
      equal(playTone.icons, 1);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Poll Interval
 */
add_test(function test_stk_proactive_command_poll_interval() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x0D,
      0x81, 0x03, 0x01, 0x03, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x84, 0x02, 0x01, 0x14
    ],
    typeOfCommand: STK_CMD_POLL_INTERVAL,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let interval = cmdDetails.options;

      equal(interval.timeUnit, STK_TIME_UNIT_SECOND);
      equal(interval.timeInterval, 0x14);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command: Display Text
 */
add_test(function test_stk_proactive_command_display_text() {
  test_stk_proactive_command({
    pdu: [
      0xd0,
      0x2c,
      0x81, 0x03, 0x01, 0x21, 0x80,
      0x82, 0x02, 0x81, 0x02,
      0x0d, 0x1d, 0x00, 0xd3, 0x30, 0x9b, 0xfc, 0x06, 0xc9, 0x5c, 0x30, 0x1a,
      0xa8, 0xe8, 0x02, 0x59, 0xc3, 0xec, 0x34, 0xb9, 0xac, 0x07, 0xc9, 0x60,
      0x2f, 0x58, 0xed, 0x15, 0x9b, 0xb9, 0x40,
      0x9e, 0x02, 0x00, 0x01
    ],
    typeOfCommand: STK_CMD_DISPLAY_TEXT,
    icons: [1],
    testFunc: (context, cmdDetails, ctlvs) => {
      let textMsg = cmdDetails.options;

      equal(textMsg.text, "Saldo 2.04 E. Validez 20/05/13. ");
      equal(textMsg.iconSelfExplanatory, true);
      equal(textMsg.icons, 1);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command: Set Up Event List.
 */
add_test(function test_stk_proactive_command_event_list() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x0F,
      0x81, 0x03, 0x01, 0x05, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x99, 0x04, 0x00, 0x01, 0x02, 0x03
    ],
    typeOfCommand: STK_CMD_SET_UP_EVENT_LIST,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let event = cmdDetails.options;

      equal(Array.isArray(event.eventList), true);

      for (let i = 0; i < event.eventList.length; i++) {
        equal(event.eventList[i], i);
      }
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Get Input
 */
add_test(function test_stk_proactive_command_get_input() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x22,
      0x81, 0x03, 0x01, 0x23, 0x8F,
      0x82, 0x02, 0x81, 0x82,
      0x8D, 0x05, 0x04, 0x54, 0x65, 0x78, 0x74,
      0x91, 0x02, 0x01, 0x10,
      0x17, 0x08, 0x04, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74,
      0x9E, 0x02, 0x00, 0x01
    ],
    typeOfCommand: STK_CMD_GET_INPUT,
    icons: [1],
    testFunc: (context, cmdDetails, ctlvs) => {
      let input = cmdDetails.options;

      equal(input.text, "Text");
      equal(input.isAlphabet, true);
      equal(input.isUCS2, true);
      equal(input.hideInput, true);
      equal(input.isPacked, true);
      equal(input.isHelpAvailable, true);
      equal(input.minLength, 0x01);
      equal(input.maxLength, 0x10);
      equal(input.defaultText, "Default");
      equal(input.iconSelfExplanatory, true);
      equal(input.icons, 1);
    }
  });

  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x11,
      0x81, 0x03, 0x01, 0x23, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x8D, 0x00,
      0x91, 0x02, 0x01, 0x10,
      0x17, 0x00
    ],
    typeOfCommand: STK_CMD_GET_INPUT,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let input = cmdDetails.options;

      equal(input.text, null);
      equal(input.minLength, 0x01);
      equal(input.maxLength, 0x10);
      equal(input.defaultText, null);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : More Time
 */
add_test(function test_stk_proactive_command_more_time() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let more_time_1 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x02, 0x00,
    0x82, 0x02, 0x81, 0x82];

  for(let i = 0 ; i < more_time_1.length; i++) {
    pduHelper.writeHexOctet(more_time_1[i]);
  }

  let berTlv = berHelper.decode(more_time_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  equal(tlv.value.commandNumber, 0x01);
  equal(tlv.value.typeOfCommand, STK_CMD_MORE_TIME);
  equal(tlv.value.commandQualifier, 0x00);

  run_next_test();
});

/**
 * Verify Proactive Command : Select Item
 */
add_test(function test_stk_proactive_command_select_item() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x3D,
      0x81, 0x03, 0x01, 0x24, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
      0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
      0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
      0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
      0x18, 0x03, 0x10, 0x15, 0x20,
      0x90, 0x01, 0x01,
      0x9E, 0x02, 0x00, 0x01,
      0x9F, 0x04, 0x00, 0x01, 0x02, 0x03
    ],
    typeOfCommand: STK_CMD_SELECT_ITEM,
    icons: [1, 1, 2, 3],
    testFunc: (context, cmdDetails, ctlvs) => {
      let menu = cmdDetails.options;

      equal(menu.title, "Title");
      equal(menu.iconSelfExplanatory, true);
      equal(menu.icons, 1);
      equal(menu.items[0].identifier, 1);
      equal(menu.items[0].text, "item 1");
      equal(menu.items[0].iconSelfExplanatory, true);
      equal(menu.items[0].icons, 1);
      equal(menu.items[1].identifier, 2);
      equal(menu.items[1].text, "item 2");
      equal(menu.items[1].iconSelfExplanatory, true);
      equal(menu.items[1].icons, 2);
      equal(menu.items[2].identifier, 3);
      equal(menu.items[2].text, "item 3");
      equal(menu.items[2].iconSelfExplanatory, true);
      equal(menu.items[2].icons, 3);
      equal(menu.nextActionList[0], STK_CMD_SET_UP_CALL);
      equal(menu.nextActionList[1], STK_CMD_LAUNCH_BROWSER);
      equal(menu.nextActionList[2], STK_CMD_PLAY_TONE);
      equal(menu.defaultItem, 0x00);
    }
  });

  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x33,
      0x81, 0x03, 0x01, 0x24, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
      0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
      0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
      0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
      0x18, 0x03, 0x00, 0x15, 0x81,
      0x90, 0x01, 0x03
    ],
    typeOfCommand: STK_CMD_SELECT_ITEM,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let menu = cmdDetails.options;

      equal(menu.title, "Title");
      equal(menu.items[0].identifier, 1);
      equal(menu.items[0].text, "item 1");
      equal(menu.items[1].identifier, 2);
      equal(menu.items[1].text, "item 2");
      equal(menu.items[2].identifier, 3);
      equal(menu.items[2].text, "item 3");
      equal(menu.nextActionList[0], STK_NEXT_ACTION_NULL);
      equal(menu.nextActionList[1], STK_CMD_LAUNCH_BROWSER);
      equal(menu.nextActionList[2], STK_NEXT_ACTION_END_PROACTIVE_SESSION);
      equal(menu.defaultItem, 0x02);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Set Up Menu
 */
add_test(function test_stk_proactive_command_set_up_menu() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x3A,
      0x81, 0x03, 0x01, 0x25, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
      0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
      0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
      0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
      0x18, 0x03, 0x10, 0x15, 0x20,
      0x9E, 0x02, 0x00, 0x01,
      0x9F, 0x04, 0x00, 0x01, 0x02, 0x03
    ],
    typeOfCommand: STK_CMD_SET_UP_MENU,
    icons: [1, 1, 2, 3],
    testFunc: (context, cmdDetails, ctlvs) => {
      let menu = cmdDetails.options;

      equal(menu.title, "Title");
      equal(menu.iconSelfExplanatory, true);
      equal(menu.icons, 1);
      equal(menu.items[0].identifier, 1);
      equal(menu.items[0].text, "item 1");
      equal(menu.items[0].iconSelfExplanatory, true);
      equal(menu.items[0].icons, 1);
      equal(menu.items[1].identifier, 2);
      equal(menu.items[1].text, "item 2");
      equal(menu.items[1].iconSelfExplanatory, true);
      equal(menu.items[1].icons, 2);
      equal(menu.items[2].identifier, 3);
      equal(menu.items[2].text, "item 3");
      equal(menu.items[2].iconSelfExplanatory, true);
      equal(menu.items[2].icons, 3);
      equal(menu.nextActionList[0], STK_CMD_SET_UP_CALL);
      equal(menu.nextActionList[1], STK_CMD_LAUNCH_BROWSER);
      equal(menu.nextActionList[2], STK_CMD_PLAY_TONE);
    }
  });

  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x30,
      0x81, 0x03, 0x01, 0x25, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
      0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
      0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
      0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
      0x18, 0x03, 0x81, 0x00, 0x00
    ],
    typeOfCommand: STK_CMD_SET_UP_MENU,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let menu = cmdDetails.options;

      equal(menu.title, "Title");
      equal(menu.items[0].identifier, 1);
      equal(menu.items[0].text, "item 1");
      equal(menu.items[1].identifier, 2);
      equal(menu.items[1].text, "item 2");
      equal(menu.items[2].identifier, 3);
      equal(menu.items[2].text, "item 3");
      equal(menu.nextActionList[0], STK_NEXT_ACTION_END_PROACTIVE_SESSION);
      equal(menu.nextActionList[1], STK_NEXT_ACTION_NULL);
      equal(menu.nextActionList[2], STK_NEXT_ACTION_NULL);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Set Up Call
 */
add_test(function test_stk_proactive_command_set_up_call() {
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x31,
      0x81, 0x03, 0x01, 0x10, 0x04,
      0x82, 0x02, 0x81, 0x82,
      0x05, 0x0A, 0x44, 0x69, 0x73, 0x63, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74,
      0x86, 0x09, 0x81, 0x10, 0x32, 0x04, 0x21, 0x43, 0x65, 0x1C, 0x2C,
      0x05, 0x07, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
      0x9E, 0x02, 0x00, 0x01,
      0x9E, 0x02, 0x01, 0x02
    ],
    typeOfCommand: STK_CMD_SET_UP_CALL,
    icons: [1, 2],
    testFunc: (context, cmdDetails, ctlvs) => {
      let setupCall = cmdDetails.options;

      equal(setupCall.address, "012340123456,1,2");
      equal(setupCall.confirmMessage.text, "Disconnect");
      equal(setupCall.confirmMessage.iconSelfExplanatory, true);
      equal(setupCall.confirmMessage.icons, 1);
      equal(setupCall.callMessage.text, "Message");
      equal(setupCall.callMessage.iconSelfExplanatory, false);
      equal(setupCall.callMessage.icons, 2);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Timer Management
 */
add_test(function test_stk_proactive_command_timer_management() {
    // Timer Management - Start
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x11,
      0x81, 0x03, 0x01, 0x27, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0xA4, 0x01, 0x01,
      0xA5, 0x03, 0x10, 0x20, 0x30
    ],
    typeOfCommand: STK_CMD_TIMER_MANAGEMENT,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      equal(cmdDetails.commandQualifier, STK_TIMER_START);

      let timer = cmdDetails.options;

      equal(timer.timerId, 0x01);
      equal(timer.timerValue, (0x01 * 60 * 60) + (0x02 * 60) + 0x03);
    }
  });

  // Timer Management - Deactivate
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x0C,
      0x81, 0x03, 0x01, 0x27, 0x01,
      0x82, 0x02, 0x81, 0x82,
      0xA4, 0x01, 0x01
    ],
    typeOfCommand: STK_CMD_TIMER_MANAGEMENT,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      equal(cmdDetails.commandQualifier, STK_TIMER_DEACTIVATE);

      let timer = cmdDetails.options;

      equal(timer.timerId, 0x01);
      ok(timer.timerValue === undefined);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive Command : Provide Local Information
 */
add_test(function test_stk_proactive_command_provide_local_information() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let stkCmdHelper = context.StkCommandParamsFactory;

  // Verify IMEI
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x09,
      0x81, 0x03, 0x01, 0x26, 0x01,
      0x82, 0x02, 0x81, 0x82
    ],
    typeOfCommand: STK_CMD_PROVIDE_LOCAL_INFO,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      equal(cmdDetails.commandQualifier, STK_LOCAL_INFO_IMEI);

      let provideLocalInfo = cmdDetails.options;
      equal(provideLocalInfo.localInfoType, STK_LOCAL_INFO_IMEI);
    }
  });

  // Verify Date and Time Zone
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x09,
      0x81, 0x03, 0x01, 0x26, 0x03,
      0x82, 0x02, 0x81, 0x82
    ],
    typeOfCommand: STK_CMD_PROVIDE_LOCAL_INFO,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      equal(cmdDetails.commandQualifier, STK_LOCAL_INFO_DATE_TIME_ZONE);

      let provideLocalInfo = cmdDetails.options;
      equal(provideLocalInfo.localInfoType, STK_LOCAL_INFO_DATE_TIME_ZONE);
    }
  });

  run_next_test();
});

/**
 * Verify Proactive command : BIP Messages
 */
add_test(function test_stk_proactive_command_open_channel() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let stkCmdHelper = context.StkCommandParamsFactory;

  // Open Channel
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x0F,
      0x81, 0x03, 0x01, 0x40, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x04, 0x4F, 0x70, 0x65, 0x6E //alpha id: "Open"
    ],
    typeOfCommand: STK_CMD_OPEN_CHANNEL,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let bipMsg = cmdDetails.options;

      equal(bipMsg.text, "Open");
    }
  });

  // Close Channel
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x10,
      0x81, 0x03, 0x01, 0x41, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x05, 0x43, 0x6C, 0x6F, 0x73, 0x65 //alpha id: "Close"
    ],
    typeOfCommand: STK_CMD_CLOSE_CHANNEL,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let bipMsg = cmdDetails.options;

      equal(bipMsg.text, "Close");
    }
  });

  // Receive Data
  test_stk_proactive_command({
    pdu: [
      0XD0,
      0X12,
      0x81, 0x03, 0x01, 0x42, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x07, 0x52, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65 //alpha id: "Receive"
    ],
    typeOfCommand: STK_CMD_RECEIVE_DATA,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let bipMsg = cmdDetails.options;

      equal(bipMsg.text, "Receive");
    }
  });

  // Send Data
  test_stk_proactive_command({
    pdu: [
      0xD0,
      0x0F,
      0x81, 0x03, 0x01, 0x43, 0x00,
      0x82, 0x02, 0x81, 0x82,
      0x85, 0x04, 0x53, 0x65, 0x6E, 0x64 //alpha id: "Send"
    ],
    typeOfCommand: STK_CMD_SEND_DATA,
    icons: null,
    testFunc: (context, cmdDetails, ctlvs) => {
      let bipMsg = cmdDetails.options;

      equal(bipMsg.text, "Send");
    }
  });

  run_next_test();
});

/**
 * Verify Event Download Command : Location Status
 */
add_test(function test_stk_event_download_location_status() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 42 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_EVENT_LIST_SIZE(3) +
    //                      TLV_LOCATION_STATUS_SIZE(3) +
    //                      TLV_LOCATION_INFO_GSM_SIZE(9))
    equal(this.readInt32(), 42);

    // BER tag
    equal(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 19 = TLV_DEVICE_ID_SIZE(4) +
    //                  TLV_EVENT_LIST_SIZE(3) +
    //                  TLV_LOCATION_STATUS_SIZE(3) +
    //                  TLV_LOCATION_INFO_GSM_SIZE(9)
    equal(pduHelper.readHexOctet(), 19);

    // Event List, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_EVENT_TYPE_LOCATION_STATUS);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Location Status, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LOCATION_STATUS |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_SERVICE_STATE_NORMAL);

    // Location Info, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LOCATION_INFO |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 7);

    equal(pduHelper.readHexOctet(), 0x21); // MCC + MNC
    equal(pduHelper.readHexOctet(), 0x63);
    equal(pduHelper.readHexOctet(), 0x54);
    equal(pduHelper.readHexOctet(), 0); // LAC
    equal(pduHelper.readHexOctet(), 0);
    equal(pduHelper.readHexOctet(), 0); // Cell ID
    equal(pduHelper.readHexOctet(), 0);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_LOCATION_STATUS,
    locationStatus: STK_SERVICE_STATE_NORMAL,
    locationInfo: {
      mcc: "123",
      mnc: "456",
      gsmLocationAreaCode: 0,
      gsmCellId: 0
    }
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

// Test Event Download commands.

/**
 * Verify Event Download Command : Language Selection
 */
add_test(function test_stk_event_download_language_selection() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 26 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_EVENT_LIST_SIZE(3) +
    //                      TLV_LANGUAGE(4))
    equal(this.readInt32(), 26);

    // BER tag
    equal(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 19 = TLV_DEVICE_ID_SIZE(4) +
    //                  TLV_EVENT_LIST_SIZE(3) +
    //                  TLV_LANGUAGE(4)
    equal(pduHelper.readHexOctet(), 11);

    // Event List, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_EVENT_TYPE_LANGUAGE_SELECTION);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Language, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LANGUAGE);
    equal(pduHelper.readHexOctet(), 2);
    equal(iccHelper.read8BitUnpackedToString(2), "zh");

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_LANGUAGE_SELECTION,
    language: "zh"
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Download Command : User Activity
 */
add_test(function test_stk_event_download_user_activity() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 18 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3))
    equal(this.readInt32(), 18);

    // BER tag
    equal(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 7 = TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3)
    equal(pduHelper.readHexOctet(), 7);

    // Event List, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_EVENT_TYPE_USER_ACTIVITY);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_USER_ACTIVITY
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Download Command : Idle Screen Available
 */
add_test(function test_stk_event_download_idle_screen_available() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 18 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3))
    equal(this.readInt32(), 18);

    // BER tag
    equal(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 7 = TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3)
    equal(pduHelper.readHexOctet(), 7);

    // Event List, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_DISPLAY);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Downloaded Command :Browser Termination
 */
add_test(function test_stk_event_download_browser_termination() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    equal(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 24 = 2 * ( 2+TLV_DEVICE_ID(4)+TLV_EVENT_LIST_SIZE(3)
    //                        +TLV_BROWSER_TERMINATION_CAUSE(3) )
    equal(this.readInt32(), 24);

    // BER tag
    equal(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 10 = TLV_DEVICE_ID(4)+TLV_EVENT_LIST_SIZE(3)
    //                  ++TLV_BROWSER_TERMINATION_CAUSE(3)
    equal(pduHelper.readHexOctet(), 10);

    // Event List, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_EVENT_TYPE_BROWSER_TERMINATION);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 2);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    equal(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Browser Termination Case, Type-Length-Value
    equal(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_BROWSER_TERMINATION_CAUSE |
                                          COMPREHENSIONTLV_FLAG_CR);
    equal(pduHelper.readHexOctet(), 1);
    equal(pduHelper.readHexOctet(), STK_BROWSER_TERMINATION_CAUSE_USER);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_BROWSER_TERMINATION,
    terminationCause: STK_BROWSER_TERMINATION_CAUSE_USER
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});
