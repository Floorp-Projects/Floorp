/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

const CB_MESSAGE_SIZE_ETWS = 56;

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

SpecialPowers.addPermission("cellbroadcast", true, document);
SpecialPowers.addPermission("mobileconnection", true, document);

let cbs = window.navigator.mozCellBroadcast;
ok(cbs instanceof MozCellBroadcast,
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
function testEtwsMessageAttributes() {
  log("Test ETWS Primary Notification message attributes");

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
    ok('language' in message, "message.language");
    ok(message.language == null, "message.language");
    ok('body' in message, "message.body");
    ok(message.body == null, "message.body");
    is(message.messageClass, "normal", "message.messageClass");
    ok(message.timestamp != null, "message.timestamp");
    ok(message.etws != null, "message.etws");
    ok(message.etws.warningType != null, "message.etws.warningType");
    ok(message.etws.emergencyUserAlert != null,
       "message.etws.emergencyUserAlert");
    ok(message.etws.popup != null, "message.etws.popup");

    window.setTimeout(testReceiving_ETWS_GeographicalScope, 0);
  });

  // Here we use a simple ETWS message for test.
  let pdu = buildHexStr(0, CB_MESSAGE_SIZE_ETWS * 2); // 6 octets
  sendCellBroadcastMessage(pdu);
}

function testReceiving_ETWS_GeographicalScope() {
  log("Test receiving ETWS Primary Notification - Geographical Scope");

  function do_test(gs, nextTest) {
    // Here we use a simple ETWS message for test.
    let pdu = buildHexStr(((gs & 0x03) << 14), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 2) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.geographicalScope, CB_GSM_GEOGRAPHICAL_SCOPE_NAMES[gs],
         "message.geographicalScope");
    });
  }

  repeat(do_test, seq(CB_GSM_GEOGRAPHICAL_SCOPE_NAMES.length),
         testReceiving_ETWS_MessageCode);
}

function testReceiving_ETWS_MessageCode() {
  log("Test receiving ETWS Primary Notification - Message Code");

  // Message Code has 10 bits, and is ORed into a 16 bits 'serial' number. Here
  // we test every single bit to verify the operation doesn't go wrong.
  let messageCodesToTest = [
    0x000, 0x001, 0x002, 0x004, 0x008, 0x010, 0x020, 0x040,
    0x080, 0x100, 0x200, 0x251
  ];

  function do_test(messageCode, nextTest) {
    let pdu = buildHexStr(((messageCode & 0x3FF) << 4), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 2) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.messageCode, messageCode, "message.messageCode");
    });
  }

  repeat(do_test, messageCodesToTest, testReceiving_ETWS_MessageId);
}

function testReceiving_ETWS_MessageId() {
  log("Test receiving ETWS Primary Notification - Message Identifier");

  // Message Identifier has 16 bits, but no bitwise operation is needed.
  // Test some selected values only.
  let messageIdsToTest = [
    0x0000, 0x0001, 0x0010, 0x0100, 0x1000, 0x1111, 0x8888, 0x8811
  ];

  function do_test(messageId, nextTest) {
    let pdu = buildHexStr(0, 4)
            + buildHexStr((messageId & 0xFFFF), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 4) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      is(message.messageId, messageId, "message.messageId");
    });
  }

  repeat(do_test, messageIdsToTest, testReceiving_ETWS_Timestamp);
}

function testReceiving_ETWS_Timestamp() {
  log("Test receiving ETWS Primary Notification - Timestamp");

  // Here we use a simple ETWS message for test.
  let pdu = buildHexStr(0, 12); // 6 octets
  doTestHelper(pdu, testReceiving_ETWS_WarningType, function (message) {
    // Cell Broadcast messages do not contain a timestamp field (however, ETWS
    // does). We only check the timestamp doesn't go too far (60 seconds) here.
    let msMessage = message.timestamp.getTime();
    let msNow = Date.now();
    ok(Math.abs(msMessage - msNow) < (1000 * 60), "message.timestamp");
  });
}

function testReceiving_ETWS_WarningType() {
  log("Test receiving ETWS Primary Notification - Warning Type");

  // Warning Type has 7 bits, and is ORed into a 16 bits 'WarningType' field.
  // Here we test every single bit to verify the operation doesn't go wrong.
  let warningTypesToTest = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x10, 0x20, 0x40, 0x41
  ];

  function do_test(warningType, nextTest) {
    let pdu = buildHexStr(0, 8)
            + buildHexStr(((warningType & 0x7F) << 9), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 6) * 2);

    doTestHelper(pdu, nextTest, function (message) {
      ok(message.etws, "message.etws");
      is(message.etws.warningType, CB_ETWS_WARNING_TYPE_NAMES[warningType],
         "message.etws.warningType");
    });
  }

  repeat(do_test, warningTypesToTest, testReceiving_ETWS_EmergencyUserAlert);
}

function doTestEmergencyUserAlert_or_Popup(name, mask, nextTest) {
  let pdu = buildHexStr(0, 8)
          + buildHexStr(mask, 4)
          + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 6) * 2);
  doTestHelper(pdu, nextTest, function (message) {
    ok(message.etws != null, "message.etws");
    is(message.etws[name], mask != 0, "message.etws." + name);
  });
}

function testReceiving_ETWS_EmergencyUserAlert() {
  log("Test receiving ETWS Primary Notification - Emergency User Alert");

  repeat(doTestEmergencyUserAlert_or_Popup.bind(null, "emergencyUserAlert"),
         [0x100, 0x000], testReceiving_ETWS_Popup);
}

function testReceiving_ETWS_Popup() {
  log("Test receiving ETWS Primary Notification - Popup");

  repeat(doTestEmergencyUserAlert_or_Popup.bind(null, "popup"),
         [0x80, 0x000], cleanUp);
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

waitFor(testEtwsMessageAttributes, function () {
  return navigator.mozMobileConnection.voice.connected;
});

