/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let manager = window.navigator.mozMobileMessage;
let smsId;

function verifyInitialState() {
  log("Verifying initial state.");
  ok(manager instanceof MozMobileMessageManager,
     "manager is instance of " + manager.constructor);
  simulateIncomingSms();
}

function simulateIncomingSms() {
  let text = "Incoming SMS courtesy of Firefox OS";
  let remoteNumber = "5557779999";

  log("Simulating incoming SMS.");

  // Simulate incoming SMS sent from remoteNumber to our emulator
  rcvdEmulatorCallback = false;
  runEmulatorCmd("sms send " + remoteNumber + " " + text, function(result) {
    is(result[0], "OK", "emulator callback");
    rcvdEmulatorCallback = true;
  });
}

// Callback for incoming SMS
manager.onreceived = function onreceived(event) {
  log("Received 'onreceived' sms event.");
  let incomingSms = event.message;
  log("Received SMS (id: " + incomingSms.id + ").");
  is(incomingSms.read, false, "incoming message read");
  smsId = incomingSms.id;

  // Wait for emulator to catch up before continuing
  waitFor(test1, function() {
    return(rcvdEmulatorCallback);
  });
};

function markMsgError(invalidId, readBool, nextFunction) {
  let requestRet = manager.markMessageRead(invalidId, readBool);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event, but expected error.");
    ok(false, "Smsrequest should have returned error but did not");
    nextFunction();
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    nextFunction();
  };
}

function test1() {
  // Mark message read for a message that doesn't exist, expect error
  let msgIdNoExist = smsId + 1;
  log("Attempting to mark non-existent sms (id: " + msgIdNoExist
      + ") read, expect error.");
  markMsgError(msgIdNoExist, true, test2);
}

function test2() {
  // Mark message read using invalid SMS id, expect error
  invalidId = -1;
  log("Attempting to mark sms unread using an invalid id (id: " + invalidId
      + "), expect error.");
  markMsgError(invalidId, false, deleteMsg);
}

function deleteMsg() {
  log("Deleting SMS (id: " + smsId + ").");
  let request = manager.delete(smsId);
  ok(request instanceof DOMRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      // Message deleted
      cleanUp();
    } else {
      log("SMS delete failed.");
      ok(false,"manager.delete request returned false");
      cleanUp();
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.delete request returned unexpected error: "
        + event.target.error.name );
    cleanUp();
  };
}

function cleanUp() {
  manager.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();
