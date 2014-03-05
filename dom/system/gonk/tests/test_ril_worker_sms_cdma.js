/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function _getWorker() {
  let _postedMessage;
  let _worker = newWorker({
    postRILMessage: function(data) {
    },
    postMessage: function(message) {
      _postedMessage = message;
    }
  });
  return {
    get postedMessage() {
      return _postedMessage;
    },
    get worker() {
      return _worker;
    }
  };
}

/*
 * Helper function to covert a HEX string to a byte array.
 *
 * @param hexString
 *        A hexadecimal string of which the length is even.
 */
function hexStringToBytes(hexString) {
  let bytes = [];

  let length = hexString.length;

  for (let i = 0; i < length; i += 2) {
    bytes.push(Number.parseInt(hexString.substr(i, 2), 16));
  }

  return bytes;
}

/*
 * Helper function to covert a byte array to a HEX string.
 *
 * @param bytes
 *        Could be a regular byte array or Uint8Array.
 */
function bytesToHexString(bytes) {
  let hexString = "";
  let hex;

  for (let i = 0; i < bytes.length; i++) {
    hex = bytes[i].toString(16).toUpperCase();
    if (hex.length === 1) {
      hexString += "0";
    }
    hexString += hex;
  }

  return hexString;
}

/*
 * Helper function to ecode Opaque UserData
 *
 * @param msg_type
 *        PDU_CDMA_MSG_TYPE_SUBMIT or PDU_CDMA_MSG_TYPE_DELIVER
 * @param data
 *        The byte array of opaque data to be encoded.
 */
function encodeOpaqueUserData(bitBufferHelper, options) {
  let bearerDataBuffer = [];
  bitBufferHelper.startWrite(bearerDataBuffer);

  // Msg-Id
  bitBufferHelper.writeBits(PDU_CDMA_MSG_USERDATA_MSG_ID, 8);
  bitBufferHelper.writeBits(3, 8);
  bitBufferHelper.writeBits(options.msg_type, 4); // MSG_TYPE
  bitBufferHelper.writeBits(1, 16);               // MSG_ID
  bitBufferHelper.flushWithPadding();             // HEADER_IND (1) + RESERVED (3)

  // User Data
  bitBufferHelper.writeBits(PDU_CDMA_MSG_USERDATA_BODY, 8);
  let dataLength = options.data.length;
  bitBufferHelper.writeBits(2 + dataLength, 8);   // 2 bytes for MSG_ENCODING, NUM_FIELDS
  bitBufferHelper.writeBits(PDU_CDMA_MSG_CODING_OCTET, 5); //MSG_ENCODING
                                                  // MSG_TYPE is omitted if MSG_ENCODING is CODING_OCTET
  bitBufferHelper.writeBits(dataLength, 8);       // NUM_FIELDS
  for (let i = 0; i < dataLength; i++) {          // CHARi
    bitBufferHelper.writeBits(options.data[i], 8);
  }
  bitBufferHelper.flushWithPadding();             // RESERVED (3 filling bits)

  return bearerDataBuffer;
}

function newSmsParcel(cdmaPduHelper, pdu) {
  return newIncomingParcel(-1,
                           RESPONSE_TYPE_UNSOLICITED,
                           UNSOLICITED_RESPONSE_CDMA_NEW_SMS,
                           pduToParcelData(cdmaPduHelper, pdu));
}

/*
 * Helper function to encode PDU into Parcel.
 * See ril_cdma_sms.h for the structure definition of RIL_CDMA_SMS_Message
 *
 * @param teleservice
 *        The Teleservice-Id of this PDU.
 *        See PDU_CDMA_MSG_TELESERIVCIE_ID_XXX in ril_const.js.
 * @param address
 *        The Orginating or Destinating address.
 * @param bearerData
 *        The byte array of the encoded bearer data.
 */
function pduToParcelData(cdmaPduHelper, pdu) {

  let addrInfo = cdmaPduHelper.encodeAddr(pdu.address);
  // Teleservice, isServicePresent, ServiceCategory,
  // addrInfo {digitMode, numberMode, numberType, numberPlan, address.length, address}
  // Sub Address
  // bearerData length, bearerData.
  let dataLength = 4 + 4 + 4
                 + (5 + addrInfo.address.length) * 4
                 + 3 * 4
                 + 4 + pdu.bearerData.length * 4;

  let data = new Uint8Array(dataLength);
  let offset = 0;

  function writeInt(value) {
    data[offset++] = value & 0xFF;
    data[offset++] = (value >>  8) & 0xFF;
    data[offset++] = (value >> 16) & 0xFF;
    data[offset++] = (value >> 24) & 0xFF;
  }

  function writeByte(value) {
    data[offset++] = value & 0xFF;
    data[offset++] = 0;
    data[offset++] = 0;
    data[offset++] = 0;
  }

  // Teleservice
  writeInt(pdu.teleservice);

  // isServicePresent
  writeByte(0);

  // ServiceCategory
  writeInt(PDU_CDMA_MSG_CATEGORY_UNSPEC);

  // AddrInfo
  writeByte(addrInfo.digitMode);
  writeByte(addrInfo.numberMode);
  writeByte(addrInfo.numberType);
  writeByte(addrInfo.numberPlan);
  let addressLength = addrInfo.address.length;
  writeByte(addressLength);
  for (let i = 0; i < addressLength; i++) {
    writeByte(addrInfo.address[i]);
  }

  // Subaddress
  writeByte(0);
  writeByte(0);
  writeByte(0);

  // Bearer Data Length
  let dataLength = pdu.bearerData.length;
  writeByte(dataLength);

  // Bearer Data
  for (let i = 0; i < dataLength; i++) {
    writeByte(pdu.bearerData[i]);
  }

  return data;
}

/**
 * Verify CDMA SMS Delivery ACK Message.
 */
add_test(function test_processCdmaSmsStatusReport() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  function test_StatusReport(errorClass, msgStatus) {
    let msgId = 0;
    let sentSmsMap = context.RIL._pendingSentSmsMap;

    sentSmsMap[msgId] = {};

    let message = {
      SMSC:             "",
      mti:              0,
      udhi:             0,
      sender:           "0987654321",
      recipient:        null,
      pid:              PDU_PID_DEFAULT,
      epid:             PDU_PID_DEFAULT,
      dcs:              0,
      mwi:              null,
      replace:          false,
      header:           null,
      body:             "Status: Sent, Dest: 0987654321",
      data:             null,
      timestamp:        new Date().valueOf(),
      language:         null,
      status:           null,
      scts:             null,
      dt:               null,
      encoding:         PDU_CDMA_MSG_CODING_7BITS_ASCII,
      messageClass:     GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
      messageType:      PDU_CDMA_MSG_TYPE_P2P,
      serviceCategory:  0,
      subMsgType:       PDU_CDMA_MSG_TYPE_DELIVER_ACK,
      msgId:            msgId,
      errorClass:       errorClass,
      msgStatus:        msgStatus
    };

    context.RIL._processCdmaSmsStatusReport(message);

    let postedMessage = workerHelper.postedMessage;

    // Check if pending token is removed.
    do_check_true((errorClass === 2) ? !!sentSmsMap[msgId] : !sentSmsMap[msgId]);

    // Check the response message accordingly.
    if (errorClass === -1) {
      // Check if the report is treated as normal incoming SMS
      do_check_eq("sms-received", postedMessage.rilMessageType);
    } else if (errorClass === 2) {
      // Do nothing.
    } else {
      // Check Delivery Status
      if (errorClass === 0) {
        do_check_eq(postedMessage.deliveryStatus, GECKO_SMS_DELIVERY_STATUS_SUCCESS);
      } else {
        do_check_eq(postedMessage.deliveryStatus, GECKO_SMS_DELIVERY_STATUS_ERROR);
      }
    }
  }

  test_StatusReport(-1, -1); // Message Status Sub-parameter is absent.
  test_StatusReport(0, 0);   // 00|000000: no error|Message accepted
  test_StatusReport(2, 4);   // 10|000100: temporary condition|Network congestion
  test_StatusReport(3, 5);   // 11|000101: permanent condition|Network error

  run_next_test();
});

/**
 * Verify WAP Push over CDMA SMS Message.
 */
add_test(function test_processCdmaSmsWapPush() {
  let workerHelper = _getWorker(),
      worker = workerHelper.worker,
      context = worker.ContextPool._contexts[0],
      bitBufferHelper = context.BitBufferHelper,
      cdmaPduHelper = context.CdmaPDUHelper;

  function test_CdmaSmsWapPdu(wdpData, reversed) {
    let orig_address = "0987654321",
        hexString,
        fullDataHexString = "";

    for (let i = 0; i < wdpData.length; i++) {
      let dataIndex = (reversed) ? (wdpData.length - i - 1) : i;
      hexString  = "00";                                // MSG_TYPE
      hexString += bytesToHexString([wdpData.length]);  // TOTAL_SEG
      hexString += bytesToHexString([dataIndex]);       // SEG_NUM (zero-based)
      if ((dataIndex === 0)) {
        hexString += "23F00B84";                        // SOURCE_PORT, DEST_PORT for 1st segment
      }
      hexString += wdpData[dataIndex];                  // WDP DATA

      do_print("hexString: " + hexString);

      fullDataHexString += wdpData[i];

      let pdu = {
        teleservice: PDU_CDMA_MSG_TELESERIVCIE_ID_WAP,
        address:     orig_address,
        bearerData:  encodeOpaqueUserData(bitBufferHelper,
                                          { msg_type: PDU_CDMA_MSG_TYPE_DELIVER,
                                            data:     hexStringToBytes(hexString) })
      };

      worker.onRILMessage(0, newSmsParcel(cdmaPduHelper, pdu));
    }

    let postedMessage = workerHelper.postedMessage;

    do_print("fullDataHexString: " + fullDataHexString);

    do_check_eq("sms-received", postedMessage.rilMessageType);
    do_check_eq(PDU_CDMA_MSG_TELESERIVCIE_ID_WAP, postedMessage.teleservice);
    do_check_eq(orig_address, postedMessage.sender);
    do_check_eq(0x23F0, postedMessage.header.originatorPort);
    do_check_eq(0x0B84, postedMessage.header.destinationPort);
    do_check_eq(fullDataHexString, bytesToHexString(postedMessage.data));
  }

  // Verify Single WAP PDU
  test_CdmaSmsWapPdu(["000102030405060708090A0B0C0D0E0F"]);

  run_next_test();
});
