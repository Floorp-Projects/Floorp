/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function testReceiving_ETWS_MessageAttributes() {
  log("Test receiving ETWS Primary Notification - Message Attributes");

  let verifyCBMessage = (aMessage) => {
    // Attributes other than `language` and `body` should always be assigned.
    ok(aMessage.gsmGeographicalScope != null, "aMessage.gsmGeographicalScope");
    ok(aMessage.messageCode != null, "aMessage.messageCode");
    ok(aMessage.messageId != null, "aMessage.messageId");
    ok('language' in aMessage, "aMessage.language");
    ok(aMessage.language == null, "aMessage.language");
    ok('body' in aMessage, "aMessage.body");
    ok(aMessage.body == null, "aMessage.body");
    is(aMessage.messageClass, "normal", "aMessage.messageClass");
    ok(aMessage.timestamp != null, "aMessage.timestamp");
    ok(aMessage.etws != null, "aMessage.etws");
    ok(aMessage.etws.warningType != null, "aMessage.etws.warningType");
    ok(aMessage.etws.emergencyUserAlert != null,
       "aMessage.etws.emergencyUserAlert");
    ok(aMessage.etws.popup != null, "aMessage.etws.popup");

    // cdmaServiceCategory shall always be unavailable in GMS/UMTS CB message.
    ok(aMessage.cdmaServiceCategory == null, "aMessage.cdmaServiceCategory");
  };

  // Here we use a simple ETWS message for test.
  let pdu = buildHexStr(0, CB_MESSAGE_SIZE_ETWS * 2); // 6 octets

  return sendMultipleRawCbsToEmulatorAndWait([pdu])
    .then((aMessage) => verifyCBMessage(aMessage));
}

function testReceiving_ETWS_GeographicalScope() {
  log("Test receiving ETWS Primary Notification - Geographical Scope");

  let promise = Promise.resolve();

  let verifyCBMessage = (aMessage, aGsName) => {
    is(aMessage.gsmGeographicalScope, aGsName,
       "aMessage.gsmGeographicalScope");
  };

  CB_GSM_GEOGRAPHICAL_SCOPE_NAMES.forEach(function(aGsName, aIndex) {
    let pdu = buildHexStr(((aIndex & 0x03) << 14), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 2) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aGsName));
  });

  return promise;
}

function testReceiving_ETWS_MessageCode() {
  log("Test receiving ETWS Primary Notification - Message Code");

  let promise = Promise.resolve();

  // Message Code has 10 bits, and is ORed into a 16 bits 'serial' number. Here
  // we test every single bit to verify the operation doesn't go wrong.
  let messageCodes = [
    0x000, 0x001, 0x002, 0x004, 0x008, 0x010, 0x020, 0x040,
    0x080, 0x100, 0x200, 0x251
  ];

  let verifyCBMessage = (aMessage, aMsgCode) => {
    is(aMessage.messageCode, aMsgCode, "aMessage.messageCode");
  };

  messageCodes.forEach(function(aMsgCode) {
    let pdu = buildHexStr(((aMsgCode & 0x3FF) << 4), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 2) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aMsgCode));
  });

  return promise;
}

function testReceiving_ETWS_MessageId() {
  log("Test receiving ETWS Primary Notification - Message Identifier");

  let promise = Promise.resolve();

  // Message Identifier has 16 bits, but no bitwise operation is needed.
  // Test some selected values only.
  let messageIds = [
    0x0000, 0x0001, 0x0010, 0x0100, 0x1000, 0x1111, 0x8888, 0x8811,
  ];

  let verifyCBMessage = (aMessage, aMessageId) => {
    is(aMessage.messageId, aMessageId, "aMessage.messageId");
  };

  messageIds.forEach(function(aMessageId) {
    let pdu = buildHexStr(0, 4)
            + buildHexStr((aMessageId & 0xFFFF), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 4) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aMessageId));
  });

  return promise;
}

function testReceiving_ETWS_Timestamp() {
  log("Test receiving ETWS Primary Notification - Timestamp");

  let verifyCBMessage = (aMessage) => {
    // Cell Broadcast messages do not contain a timestamp field (however, ETWS
    // does). We only check the timestamp doesn't go too far (60 seconds) here.
    let msMessage = aMessage.timestamp;
    let msNow = Date.now();
    ok(Math.abs(msMessage - msNow) < (1000 * 60), "aMessage.timestamp");
  };

  // Here we use a simple ETWS message for test.
  let pdu = buildHexStr(0, 12); // 6 octets

  return sendMultipleRawCbsToEmulatorAndWait([pdu])
    .then((aMessage) => verifyCBMessage(aMessage));
}

function testReceiving_ETWS_WarningType() {
  log("Test receiving ETWS Primary Notification - Warning Type");

  let promise = Promise.resolve();

  // Warning Type has 7 bits, and is ORed into a 16 bits 'WarningType' field.
  // Here we test every single bit to verify the operation doesn't go wrong.
  let warningTypes = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x10, 0x20, 0x40, 0x41
  ];

  let verifyCBMessage = (aMessage, aWarningType) => {
    ok(aMessage.etws, "aMessage.etws");
    is(aMessage.etws.warningType, CB_ETWS_WARNING_TYPE_NAMES[aWarningType],
       "aMessage.etws.warningType");
  };

  warningTypes.forEach(function(aWarningType) {
    let pdu = buildHexStr(0, 8)
            + buildHexStr(((aWarningType & 0x7F) << 9), 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 6) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aWarningType));
  });

  return promise;
}

function testReceiving_ETWS_EmergencyUserAlert() {
  log("Test receiving ETWS Primary Notification - Emergency User Alert");

  let promise = Promise.resolve();

  let emergencyUserAlertMasks = [0x100, 0x000];

  let verifyCBMessage = (aMessage, aMask) => {
    ok(aMessage.etws != null, "aMessage.etws");
    is(aMessage.etws.emergencyUserAlert, aMask != 0, "aMessage.etws.emergencyUserAlert");
  };

  emergencyUserAlertMasks.forEach(function(aMask) {
    let pdu = buildHexStr(0, 8)
            + buildHexStr(aMask, 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 6) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aMask));
  });

  return promise;
}

function testReceiving_ETWS_Popup() {
  log("Test receiving ETWS Primary Notification - Popup");

  let promise = Promise.resolve();

  let popupMasks = [0x80, 0x000];

  let verifyCBMessage = (aMessage, aMask) => {
    ok(aMessage.etws != null, "aMessage.etws");
    is(aMessage.etws.popup, aMask != 0, "aMessage.etws.popup");
  };

  popupMasks.forEach(function(aMask) {
    let pdu = buildHexStr(0, 8)
            + buildHexStr(aMask, 4)
            + buildHexStr(0, (CB_MESSAGE_SIZE_ETWS - 6) * 2);
    promise = promise
      .then(() => sendMultipleRawCbsToEmulatorAndWait([pdu]))
      .then((aMessage) => verifyCBMessage(aMessage, aMask));
  });

  return promise;
}

startTestCommon(function testCaseMain() {
  return testReceiving_ETWS_MessageAttributes()
    .then(() => testReceiving_ETWS_GeographicalScope())
    .then(() => testReceiving_ETWS_MessageCode())
    .then(() => testReceiving_ETWS_MessageId())
    .then(() => testReceiving_ETWS_Timestamp())
    .then(() => testReceiving_ETWS_WarningType())
    .then(() => testReceiving_ETWS_EmergencyUserAlert())
    .then(() => testReceiving_ETWS_Popup());
});
