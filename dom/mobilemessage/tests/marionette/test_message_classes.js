/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const PDU_SMSC = "00"; // No SMSC Address
const PDU_FIRST_OCTET = "00"; // RP:no, UDHI:no, SRI:no, MMS:no, MTI:SMS-DELIVER
const PDU_SENDER = "0191F1"; // +1
const PDU_PID_NORMAL = "00";
const PDU_PID_ANSI_136_R_DATA = "7C";
const PDU_PID_USIM_DATA_DOWNLOAD = "7F";
const PDU_TIMESTAMP = "00101000000000"; // 2000/01/01
const PDU_UDL = "01";
const PDU_UD = "41";

SpecialPowers.addPermission("sms", true, document);

let sms = window.navigator.mozSms;
ok(sms instanceof MozSmsManager);

let pendingEmulatorCmdCount = 0;
function sendSmsPduToEmulator(pdu) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms pdu " + pdu;
  runEmulatorCmd(cmd, function (result) {
    --pendingEmulatorCmdCount;

    is(result[0], "OK", "Emulator response");
  });
}

const TIMESTAMP = Date.UTC(2000, 0, 1);
function checkMessage(message, id, threadId, messageClass) {
  ok(message instanceof MozSmsMessage,
     "message is instanceof " + message.constructor);
  if (id == null) {
    ok(message.id > 0, "message.id");
  } else {
    is(message.id, id, "message.id");
  }
  if (threadId == null) {
    ok(message.threadId > 0, "message.threadId");
  } else {
    is(message.threadId, threadId, "message.threadId");
  }
  is(message.delivery, "received", "message.delivery");
  is(message.deliveryStatus, "success", "message.deliveryStatus");
  is(message.sender, "+1", "message.sender");
  is(message.body, "A", "message.body");
  is(message.messageClass, messageClass, "message.messageClass");
  ok(message.timestamp instanceof Date,
     "message.timestamp is instanceof " + message.timestamp.constructor);
  is(message.timestamp.getTime(), TIMESTAMP, "message.timestamp");
  is(message.read, false, "message.read");
}

function test_message_class_0() {
  let allDCSs = [
    "10", // General Group: 00xx
    "50", // Automatica Deletion Group: 01xx
    "F0"  // (no name) Group: 1111
  ];

  function do_test(dcsIndex) {
    sms.addEventListener("received", function onReceived(event) {
      sms.removeEventListener("received", onReceived);

      let message = event.message;
      checkMessage(message, -1, 0, "class-0");

      // Make sure the message is not stored.
      let request = sms.getMessages(null, false);
      request.onsuccess = function onsuccess() {
        let cursor = request.result;
        if (cursor.message) {
          // Here we check whether there is any message of the same sender.
          isnot(cursor.message.sender, message.sender, "cursor.message.sender");

          cursor.continue();
          return;
        }

        // All messages checked. Done.
        ++dcsIndex;
        if (dcsIndex >= allDCSs.length) {
          window.setTimeout(test_message_class_1, 0);
        } else {
          window.setTimeout(do_test.bind(null, dcsIndex), 0);
        }
      };
      request.onerror = function onerror() {
        ok(false, "Can't fetch messages from SMS database");
      };
    });

    let dcs = allDCSs[dcsIndex];
    log("  Testing DCS " + dcs);
    let pdu = PDU_SMSC + PDU_FIRST_OCTET + PDU_SENDER + PDU_PID_NORMAL +
              dcs + PDU_TIMESTAMP + PDU_UDL + PDU_UD;

    sendSmsPduToEmulator(pdu);
  }

  log("Checking Message Class 0");
  do_test(0);
}

function doTestMessageClassGeneric(allDCSs, messageClass, next) {
  function do_test(dcsIndex) {
    sms.addEventListener("received", function onReceived(event) {
      sms.removeEventListener("received", onReceived);

      // Make sure we can correctly receive the message
      checkMessage(event.message, null, null, messageClass);

      ++dcsIndex;
      if (dcsIndex >= allDCSs.length) {
        window.setTimeout(next, 0);
      } else {
        window.setTimeout(do_test.bind(null, dcsIndex), 0);
      }
    });

    let dcs = allDCSs[dcsIndex];
    log("  Testing DCS " + dcs);
    let pdu = PDU_SMSC + PDU_FIRST_OCTET + PDU_SENDER + PDU_PID_NORMAL +
              dcs + PDU_TIMESTAMP + PDU_UDL + PDU_UD;

    sendSmsPduToEmulator(pdu);
  }

  do_test(0);
}

function test_message_class_1() {
  let allDCSs = [
    "11", // General Group: 00xx
    "51", // Automatica Deletion Group: 01xx
    "F1"  // (no name) Group: 1111
  ];

  log("Checking Message Class 1");
  doTestMessageClassGeneric(allDCSs, "class-1", test_message_class_2);
}

function test_message_class_2() {
  let allDCSs = [
    "12", // General Group: 00xx
    "52", // Automatica Deletion Group: 01xx
    "F2"  // (no name) Group: 1111
  ];

  let allPIDs = [
    PDU_PID_NORMAL,
    PDU_PID_ANSI_136_R_DATA,
    PDU_PID_USIM_DATA_DOWNLOAD
  ];

  function do_test_dcs(dcsIndex) {
    function do_test_pid(pidIndex) {
      function onReceived(event) {
        if (pidIndex == 0) {
          // Make sure we can correctly receive the message
          checkMessage(event.message, null, null, "class-2");

          next();
          return;
        }

        // TODO: Bug 792798 - B2G SMS: develop test cases for Message Class 2
        // Since we have "data download via SMS Point-to-Point" service enabled
        // but no working implementation in emulator SIM, all class 2 messages
        // bug normal ones should goto `dataDownloadViaSMSPP()` and we should
        // not receive the message in content page.
        ok(false, "SMS-PP messages shouldn't be sent to content");
      }

      function next() {
        sms.removeEventListener("received", onReceived);

        ++pidIndex;
        if (pidIndex >= allPIDs.length) {
          ++dcsIndex;
          if (dcsIndex >= allDCSs.length) {
            window.setTimeout(test_message_class_3, 0);
          } else {
            window.setTimeout(do_test_dcs.bind(null, dcsIndex), 0);
          }
        } else {
          window.setTimeout(do_test_pid.bind(null, pidIndex), 0);
        }
      }

      sms.addEventListener("received", onReceived);

      if (pidIndex != 0) {
        // Wait for three seconds to ensure we don't receive the message.
        window.setTimeout(next, 3000);
      }

      let pid = allPIDs[pidIndex];
      log("    Testing PID " + pid);

      let pdu = PDU_SMSC + PDU_FIRST_OCTET + PDU_SENDER + pid + dcs +
                PDU_TIMESTAMP + PDU_UDL + PDU_UD;

      sendSmsPduToEmulator(pdu);
    }

    let dcs = allDCSs[dcsIndex];
    log("  Testing DCS " + dcs);

    do_test_pid(0);
  }

  log("Checking Message Class 2");
  do_test_dcs(0);
}

function test_message_class_3() {
  let allDCSs = [
    "13", // General Group: 00xx
    "53", // Automatica Deletion Group: 01xx
    "F3"  // (no name) Group: 1111
  ];

  log("Checking Message Class 3");
  doTestMessageClassGeneric(allDCSs, "class-3", cleanUp);
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("sms", document);
  finish();
}

test_message_class_0();
