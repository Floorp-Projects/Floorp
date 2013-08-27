/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

const PDU_DCS_CODING_GROUP_BITS          = 0xF0;
const PDU_DCS_MSG_CODING_7BITS_ALPHABET  = 0x00;
const PDU_DCS_MSG_CODING_8BITS_ALPHABET  = 0x04;
const PDU_DCS_MSG_CODING_16BITS_ALPHABET = 0x08;

const PDU_DCS_MSG_CLASS_BITS             = 0x03;
const PDU_DCS_MSG_CLASS_NORMAL           = 0xFF;
const PDU_DCS_MSG_CLASS_0                = 0x00;
const PDU_DCS_MSG_CLASS_ME_SPECIFIC      = 0x01;
const PDU_DCS_MSG_CLASS_SIM_SPECIFIC     = 0x02;
const PDU_DCS_MSG_CLASS_TE_SPECIFIC      = 0x03;
const PDU_DCS_MSG_CLASS_USER_1           = 0x04;
const PDU_DCS_MSG_CLASS_USER_2           = 0x05;

const GECKO_SMS_MESSAGE_CLASSES = {};
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]       = "normal";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]            = "class-0";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_ME_SPECIFIC]  = "class-1";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_SIM_SPECIFIC] = "class-2";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_TE_SPECIFIC]  = "class-3";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_USER_1]       = "user-1";
GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_USER_2]       = "user-2";

const CB_MESSAGE_SIZE_GSM  = 88;

const CB_GSM_MESSAGEID_ETWS_BEGIN = 0x1100;
const CB_GSM_MESSAGEID_ETWS_END   = 0x1107;

const CB_GSM_GEOGRAPHICAL_SCOPE_NAMES = [
  "cell-immediate",
  "plmn",
  "location-area",
  "cell"
];

const CB_ETWS_WARNING_TYPE_NAMES = [
  "earthquake",
  "tsunami",
  "earthquake-tsunami",
  "test",
  "other"
];

const CB_DCS_LANG_GROUP_1 = [
  "de", "en", "it", "fr", "es", "nl", "sv", "da", "pt", "fi",
  "no", "el", "tr", "hu", "pl", null
];
const CB_DCS_LANG_GROUP_2 = [
  "cs", "he", "ar", "ru", "is", null, null, null, null, null,
  null, null, null, null, null, null
];

const CB_MAX_CONTENT_7BIT = Math.floor((CB_MESSAGE_SIZE_GSM - 6) * 8 / 7);
const CB_MAX_CONTENT_UCS2 = Math.floor((CB_MESSAGE_SIZE_GSM - 6) / 2);

const BODY_7BITS = "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                 + "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                 + "@@@@@@@@@@@@@"; // 93 ascii chars.
const BODY_7BITS_IND = BODY_7BITS.substr(3);
const BODY_UCS2 = "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"
                + "\u0000"; // 41 unicode chars.
const BODY_UCS2_IND = BODY_UCS2.substr(1);

SpecialPowers.addPermission("cellbroadcast", true, document);
SpecialPowers.addPermission("mobileconnection", true, document);

is(BODY_7BITS.length,     CB_MAX_CONTENT_7BIT,     "BODY_7BITS.length");
is(BODY_7BITS_IND.length, CB_MAX_CONTENT_7BIT - 3, "BODY_7BITS_IND.length");
is(BODY_UCS2.length,      CB_MAX_CONTENT_UCS2,     "BODY_UCS2.length");
is(BODY_UCS2_IND.length,  CB_MAX_CONTENT_UCS2 - 1, "BODY_UCS2_IND.length")

let cbs = window.navigator.mozCellBroadcast;
ok(cbs instanceof window.MozCellBroadcast,
   "mozCellBroadcast is instanceof " + cbs.constructor);

let pendingEmulatorCmdCount = 0;
function sendCellBroadcastMessage(pdu, callback) {
  pendingEmulatorCmdCount++;

  let cmd = "cbs pdu " + pdu;
  runEmulatorCmd(cmd, function (result) {
    pendingEmulatorCmdCount--;

    is(result[0], "OK", "Emulator response");

    if (callback) {
      window.setTimeout(callback, 0);
    }
  });
}

function buildHexStr(n, numSemiOctets) {
  let str = n.toString(16);
  ok(str.length <= numSemiOctets);
  while (str.length < numSemiOctets) {
    str = "0" + str;
  }
  return str;
}

function seq(end, begin) {
  let result = [];
  for (let i = begin || 0; i < end; i++) {
    result.push(i);
  }
  return result;
}

function repeat(func, array, oncomplete) {
  (function do_call(index) {
    let next = index < (array.length - 1) ? do_call.bind(null, index + 1) : oncomplete;
    func.apply(null, [array[index], next]);
  })(0);
}

function doTestHelper(pdu, nextTest, checkFunc) {
  cbs.addEventListener("received", function onreceived(event) {
    cbs.removeEventListener("received", onreceived);

    checkFunc(event.message);

    window.setTimeout(nextTest, 0);
  });

  if (Array.isArray(pdu)) {
    repeat(sendCellBroadcastMessage, pdu);
  } else {
    sendCellBroadcastMessage(pdu);
  }
}

/**
 * Tests receiving Cell Broadcast messages, event instance type, all attributes
 * of CellBroadcastMessage exist.
 */
function testGsmMessageAttributes() {
  log("Test GSM Cell Broadcast message attributes");

  cbs.addEventListener("received", function onreceived(event) {
    cbs.removeEventListener("received", onreceived);

    // Bug 838542: following check throws an exception and fails this case.
    // ok(event instanceof MozCellBroadcastEvent,
    //    "event is instanceof " + event.constructor)
    ok(event, "event is valid");

    let message = event.message;
    ok(message, "event.message is valid");

    // Attributes other than `language` and `body` should always be assigned.
    ok(message.geographicalScope != null, "message.geographicalScope");
    ok(message.messageCode != null, "message.messageCode");
    ok(message.messageId != null, "message.messageId");
    ok(message.language != null, "message.language");
    ok(message.body != null, "message.body");
    ok(message.messageClass != null, "message.messageClass");
    ok(message.timestamp != null, "message.timestamp");
    ok('etws' in message, "message.etws");
    if (message.etws) {
      ok('warningType' in message.etws, "message.etws.warningType");
      ok(message.etws.emergencyUserAlert != null, "message.etws.emergencyUserAlert");
      ok(message.etws.popup != null, "message.etws.popup");
    }

    window.setTimeout(testReceiving_GSM_GeographicalScope, 0);
  });

  // Here we use a simple GSM message for test.
  let pdu = buildHexStr(0, CB_MESSAGE_SIZE_GSM * 2);
  sendCellBroadcastMessage(pdu);
}

function testReceiving_GSM_GeographicalScope() {
  log("Test receiving GSM Cell Broadcast - Geographical Scope");

  function do_test(gs, nextTest) {
    let pdu = buildHexStr(((gs & 0x03) << 14), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 2) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.geographicalScope, CB_GSM_GEOGRAPHICAL_SCOPE_NAMES[gs],
         "message.geographicalScope");
    });
  }

  repeat(do_test, seq(CB_GSM_GEOGRAPHICAL_SCOPE_NAMES.length),
         testReceiving_GSM_MessageCode);
}

function testReceiving_GSM_MessageCode() {
  log("Test receiving GSM Cell Broadcast - Message Code");

  // Message Code has 10 bits, and is ORed into a 16 bits 'serial' number. Here
  // we test every single bit to verify the operation doesn't go wrong.
  let messageCodesToTest = [
    0x000, 0x001, 0x002, 0x004, 0x008, 0x010, 0x020, 0x040,
    0x080, 0x100, 0x200, 0x251
  ];

  function do_test(messageCode, nextTest) {
    let pdu = buildHexStr(((messageCode & 0x3FF) << 4), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 2) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.messageCode, messageCode, "message.messageCode");
    });
  }

  repeat(do_test, messageCodesToTest, testReceiving_GSM_MessageId);
}

function testReceiving_GSM_MessageId() {
  log("Test receiving GSM Cell Broadcast - Message Identifier");

  // Message Identifier has 16 bits, but no bitwise operation is needed.
  // Test some selected values only.
  let messageIdsToTest = [
    0x0000, 0x0001, 0x0010, 0x0100, 0x1000, 0x1111, 0x8888, 0x8811,
  ];

  function do_test(messageId, nextTest) {
    let pdu = buildHexStr(0, 4)
            + buildHexStr((messageId & 0xFFFF), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 4) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.messageId, messageId, "message.messageId");
      ok(message.etws == null, "message.etws");
    });
  }

  repeat(do_test, messageIdsToTest, testReceiving_GSM_Language_and_Body);
}

// Copied from GsmPDUHelper.readCbDataCodingScheme
function decodeDataCodingScheme(dcs) {
  let language = null;
  let hasLanguageIndicator = false;
  let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
  let messageClass = PDU_DCS_MSG_CLASS_NORMAL;

  switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
    case 0x00: // 0000
      language = CB_DCS_LANG_GROUP_1[dcs & 0x0F];
      break;

    case 0x10: // 0001
      switch (dcs & 0x0F) {
        case 0x00:
          hasLanguageIndicator = true;
          break;
        case 0x01:
          encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
          hasLanguageIndicator = true;
          break;
      }
      break;

    case 0x20: // 0010
      language = CB_DCS_LANG_GROUP_2[dcs & 0x0F];
      break;

    case 0x40: // 01xx
    case 0x50:
    //case 0x60:
    //case 0x70:
    case 0x90: // 1001
      encoding = (dcs & 0x0C);
      if (encoding == 0x0C) {
        encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
      }
      messageClass = (dcs & PDU_DCS_MSG_CLASS_BITS);
      break;

    case 0xF0:
      encoding = (dcs & 0x04) ? PDU_DCS_MSG_CODING_8BITS_ALPHABET
                              : PDU_DCS_MSG_CODING_7BITS_ALPHABET;
      switch(dcs & PDU_DCS_MSG_CLASS_BITS) {
        case 0x01: messageClass = PDU_DCS_MSG_CLASS_USER_1; break;
        case 0x02: messageClass = PDU_DCS_MSG_CLASS_USER_2; break;
        case 0x03: messageClass = PDU_DCS_MSG_CLASS_TE_SPECIFIC; break;
      }
      break;

    case 0x30: // 0011 (Reserved)
    case 0x80: // 1000 (Reserved)
    case 0xA0: // 1010..1100 (Reserved)
    case 0xB0:
    case 0xC0:
      break;
    default:
      throw new Error("Unsupported CBS data coding scheme: " + dcs);
  }

  return [encoding, language, hasLanguageIndicator,
          GECKO_SMS_MESSAGE_CLASSES[messageClass]];
}

function testReceiving_GSM_Language_and_Body() {
  log("Test receiving GSM Cell Broadcast - Language & Body");

  function do_test(dcs) {
    let encoding, language, indicator, messageClass;
    try {
      [encoding, language, indicator, messageClass] = decodeDataCodingScheme(dcs);
    } catch (e) {
      // Unsupported coding group, skip.
      let nextGroup = (dcs & PDU_DCS_CODING_GROUP_BITS) + 0x10;
      window.setTimeout(do_test.bind(null, nextGroup), 0);
      return;
    }

    let pdu = buildHexStr(0, 8)
            + buildHexStr(dcs, 2)
            + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 5) * 2);

    let nextTest = (dcs < 0xFF) ? do_test.bind(null, dcs + 1)
                                 : testReceiving_GSM_Timestamp;
    doTestHelper(pdu, nextTest, function (message) {
      if (language) {
        is(message.language, language, "message.language");
      } else if (indicator) {
        is(message.language, "@@", "message.language");
      } else {
        ok(message.language == null, "message.language");
      }

      switch (encoding) {
        case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
          is(message.body, indicator ? BODY_7BITS_IND : BODY_7BITS, "message.body");
          break;
        case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
          ok(message.body == null, "message.body");
          break;
        case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
          is(message.body, indicator ? BODY_UCS2_IND : BODY_UCS2, "message.body");
          break;
      }

      is(message.messageClass, messageClass, "message.messageClass");
    });
  }

  do_test(0);
}

function testReceiving_GSM_Timestamp() {
  log("Test receiving GSM Cell Broadcast - Timestamp");

  let pdu = buildHexStr(0, CB_MESSAGE_SIZE_GSM * 2);
  doTestHelper(pdu, testReceiving_GSM_WarningType, function (message) {
    // Cell Broadcast messages do not contain a timestamp field (however, ETWS
    // does). We only check the timestamp doesn't go too far (60 seconds) here.
    let msMessage = message.timestamp.getTime();
    let msNow = Date.now();
    ok(Math.abs(msMessage - msNow) < (1000 * 60), "message.timestamp");
  });
}

function testReceiving_GSM_WarningType() {
  log("Test receiving GSM Cell Broadcast - Warning Type");

  let messageIdsToTest = [];
  for (let i = CB_GSM_MESSAGEID_ETWS_BEGIN; i <= CB_GSM_MESSAGEID_ETWS_END; i++) {
    messageIdsToTest.push(i);
  }

  function do_test(messageId, nextTest) {
    let pdu = buildHexStr(0, 4)
            + buildHexStr((messageId & 0xFFFF), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 4) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.messageId, messageId, "message.messageId");
      ok(message.etws != null, "message.etws");

      let offset = messageId - CB_GSM_MESSAGEID_ETWS_BEGIN;
      if (offset < CB_ETWS_WARNING_TYPE_NAMES.length) {
        is(message.etws.warningType, CB_ETWS_WARNING_TYPE_NAMES[offset],
           "message.etws.warningType");
      } else {
        ok(message.etws.warningType == null, "message.etws.warningType");
      }
    });
  }

  repeat(do_test, messageIdsToTest, testReceiving_GSM_EmergencyUserAlert);
}

function doTestEmergencyUserAlert_or_Popup(name, mask, nextTest) {
  let pdu = buildHexStr(mask, 4)
          + buildHexStr(CB_GSM_MESSAGEID_ETWS_BEGIN, 4)
          + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 4) * 2);

  doTestHelper(pdu, nextTest, function (message) {
    is(message.messageId, CB_GSM_MESSAGEID_ETWS_BEGIN, "message.messageId");
    ok(message.etws != null, "message.etws");
    is(message.etws[name], mask != 0, "message.etws." + name);
  });
}

function testReceiving_GSM_EmergencyUserAlert() {
  log("Test receiving GSM Cell Broadcast - Emergency User Alert");

  repeat(doTestEmergencyUserAlert_or_Popup.bind(null, "emergencyUserAlert"),
         [0x2000, 0x0000], testReceiving_GSM_Popup);
}

function testReceiving_GSM_Popup() {
  log("Test receiving GSM Cell Broadcast - Popup");

  repeat(doTestEmergencyUserAlert_or_Popup.bind(null, "popup"),
         [0x1000, 0x0000], testReceiving_GSM_Multipart);
}

function testReceiving_GSM_Multipart() {
  log("Test receiving GSM Cell Broadcast - Multipart Messages");

  function do_test(numParts, nextTest) {
    let pdus = [];
    for (let i = 1; i <= numParts; i++) {
      let pdu = buildHexStr(0, 10)
              + buildHexStr((i << 4) + numParts, 2)
              + buildHexStr(0, (CB_MESSAGE_SIZE_GSM - 6) * 2);
      pdus.push(pdu);
    }

    doTestHelper(pdus, nextTest, function (message) {
      is(message.body.length, (numParts * CB_MAX_CONTENT_7BIT),
         "message.body");
    });
  }

  repeat(do_test, seq(16, 1), cleanUp);
}

function cleanUp() {
  if (pendingEmulatorCmdCount > 0) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  SpecialPowers.removePermission("cellbroadcast", true, document);

  finish();
}

waitFor(testGsmMessageAttributes, function () {
  return navigator.mozMobileConnection.voice.connected;
});

