/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function buildHexStr(aNum, aNumSemiOctets) {
  let str = aNum.toString(16);
  while (str.length < aNumSemiOctets) {
    str = "0" + str;
  }
  return str;
}

function hexStringToParcelByteArrayData(hexString) {
  let bytes = [];

  let length = hexString.length / 2;

  bytes.push(length & 0xFF);
  bytes.push((length >>  8) & 0xFF);
  bytes.push((length >> 16) & 0xFF);
  bytes.push((length >> 24) & 0xFF);

  for (let i = 0; i < hexString.length; i += 2) {
    bytes.push(Number.parseInt(hexString.substr(i, 2), 16));
  }

  return bytes;
}

/**
 * Verify GsmPDUHelper#readUmtsCbMessage with numOfPages from 1 to 15.
 */
add_test(function test_GsmPDUHelper_readUmtsCbMessage_MultiParts() {
  let CB_UMTS_MESSAGE_PAGE_SIZE = 82;
  let CB_MAX_CONTENT_PER_PAGE_7BIT = 93;
  let workerHelper = newInterceptWorker(),
      worker = workerHelper.worker,
      context = worker.ContextPool._contexts[0],
      GsmPDUHelper = context.GsmPDUHelper;

  function test_MultiParts(aNumOfPages) {
    let pdu = buildHexStr(CB_UMTS_MESSAGE_TYPE_CBS, 2) // msg_type
            + buildHexStr(0, 4) // skip msg_id
            + buildHexStr(0, 4) // skip SN
            + buildHexStr(0, 2) // skip dcs
            + buildHexStr(aNumOfPages, 2); // set num_of_pages
    for (let i = 1; i <= aNumOfPages; i++) {
      pdu = pdu + buildHexStr(0, CB_UMTS_MESSAGE_PAGE_SIZE * 2)
                + buildHexStr(CB_UMTS_MESSAGE_PAGE_SIZE, 2); // msg_info_length
    }

    worker.onRILMessage(0, newIncomingParcel(-1,
                           RESPONSE_TYPE_UNSOLICITED,
                           UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS,
                           hexStringToParcelByteArrayData(pdu)));

    let postedMessage = workerHelper.postedMessage;
    equal("cellbroadcast-received", postedMessage.rilMessageType);
    equal(postedMessage.fullBody.length,
                aNumOfPages * CB_MAX_CONTENT_PER_PAGE_7BIT);
  }

  [1, 5, 15].forEach(function(i) {
    test_MultiParts(i);
  });

  run_next_test();
});

/**
 * Verify GsmPDUHelper#readUmtsCbMessage with 8bit encoded.
 */
add_test(function test_GsmPDUHelper_readUmtsCbMessage_Binary() {
  let CB_UMTS_MESSAGE_PAGE_SIZE = 82;
  let CB_MAX_CONTENT_PER_PAGE_7BIT = 93;
  let TEXT_BINARY = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                  + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                  + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                  + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                  + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                  + "FFFF";
  let workerHelper = newInterceptWorker(),
      worker = workerHelper.worker,
      context = worker.ContextPool._contexts[0],
      GsmPDUHelper = context.GsmPDUHelper;

  function test_MultiPartsBinary(aNumOfPages) {
    let pdu = buildHexStr(CB_UMTS_MESSAGE_TYPE_CBS, 2) // msg_type
            + buildHexStr(0, 4) // skip msg_id
            + buildHexStr(0, 4) // skip SN
            + buildHexStr(68, 2) // set DCS to 8bit data
            + buildHexStr(aNumOfPages, 2); // set num_of_pages
    for (let i = 1; i <= aNumOfPages; i++) {
      pdu = pdu + TEXT_BINARY
                + buildHexStr(CB_UMTS_MESSAGE_PAGE_SIZE, 2); // msg_info_length
    }

    worker.onRILMessage(0, newIncomingParcel(-1,
                           RESPONSE_TYPE_UNSOLICITED,
                           UNSOLICITED_RESPONSE_NEW_BROADCAST_SMS,
                           hexStringToParcelByteArrayData(pdu)));

    let postedMessage = workerHelper.postedMessage;
    equal("cellbroadcast-received", postedMessage.rilMessageType);
    equal(postedMessage.fullData.length,
                aNumOfPages * CB_UMTS_MESSAGE_PAGE_SIZE);
    for (let i = 0; i < postedMessage.fullData.length; i++) {
      equal(postedMessage.fullData[i], 255);
    }
  }

  [1, 5, 15].forEach(function(i) {
    test_MultiPartsBinary(i);
  });

  run_next_test();
});
