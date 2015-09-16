/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

var manager = window.navigator.mozMobileMessage;
var smsList = new Array();

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
  log("SMS read attribute: " + incomingSms.read + ".");

  // Add newly received message id to array of msgs
  smsList.push(incomingSms.id);

  // Wait for emulator to catch up before continuing
  waitFor(sendSms, function() {
    return(rcvdEmulatorCallback);
  });
};

function sendSms() {
  let gotSmsSent = false;
  let gotRequestSuccess = false;
  let remoteNumber = "5557779999";
  let text = "Mo Mo Mo Zilla Zilla Zilla!";

  log("Sending an SMS.");

  manager.onsent = function(event) {
    log("Received 'onsent' event.");
    gotSmsSent = true;
    let sentSms = event.message;
    log("Sent SMS (id: " + sentSms.id + ").");
    is(sentSms.read, true, "sent sms read");
    log("SMS read attribute: " + sentSms.read + ".");

    // Add newly received message id to array of msgs
    smsList.push(sentSms.id);

    if (gotSmsSent && gotRequestSuccess) {
      test1();
    }
  };

  let request = manager.send(remoteNumber, text);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if(event.target.result) {
      gotRequestSuccess = true;
      if (gotSmsSent && gotRequestSuccess) {
        test1();
      }
    } else {
      log("smsrequest returned false for manager.send");
      ok(false, "SMS send failed");
      deleteMsgs();
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.send request returned unexpected error: "
        + event.target.error.name );
    deleteMsgs();
  };
}

function markMessageAndVerify(smsId, readBool, nextFunction) {
  let request = manager.markMessageRead(smsId, readBool);
  ok(request instanceof DOMRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");

    // Success from MarkMessageRead, the result should match what we set
    is(event.target.result, readBool, "result matches what was set");

    // Message marked read/unread, now verify
    log("Getting SMS message (id: " + smsId + ").");
    let requestRet = manager.getMessage(smsId);
    ok(requestRet, "smsrequest obj returned");

    requestRet.onsuccess = function(event) {
      log("Received 'onsuccess' smsrequest event.");
      ok(event.target.result, "smsrequest event.target.result");
      let foundSms = event.target.result;
      is(foundSms.id, smsId, "SMS id matches");
      log("SMS read attribute: " + foundSms.read + ".");
      let text = readBool ? "read" : "unread";
      if (foundSms.read == readBool) {
        ok(true, "marked sms " + text);
      } else {
        ok(false, "marking sms " + text + " didn't work");
        log("Expected SMS (id: " + foundSms.id + ") to be marked " + text
            + " but it is not.");
      }
      nextFunction();
    };

    requestRet.onerror = function(event) {
      log("Received 'onerror' smsrequest event.");
      ok(event.target.error, "domerror obj");
      is(event.target.error.name, "NotFoundError", "error returned");
      log("Could not get SMS (id: " + outSmsId + ") but should have.");
      ok(false, "Could not get SMS");
      deleteMsgs();
    };
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.markMessageRead request returned unexpected error: "
        + event.target.error.name );
    nextFunction();
  };
}

function test1() {
  rcvdSms = smsList[0];
  log("Test 1: Marking received SMS (id: " + rcvdSms + ") read.");
  markMessageAndVerify(rcvdSms, true, test2);
}

function test2() {
  rcvdSms = smsList[0];
  log("Test 2: Marking received SMS (id: " + rcvdSms + ") unread.");
  markMessageAndVerify(rcvdSms, false, test3);
}

function test3() {
  sentSms = smsList[1];
  log("Test 3: Marking sent SMS (id: " + sentSms + ") unread.");
  markMessageAndVerify(sentSms, false, test4);
}

function test4() {
  sentSms = smsList[1];
  log("Test 4: Marking sent SMS (id: " + sentSms + ") read.");
  markMessageAndVerify(sentSms, true, test5);
}

function test5() {
  sentSms = smsList[1];
  log("Test 5: Marking an already read SMS (id: " + sentSms + ") read.");
  markMessageAndVerify(sentSms, true, test6);
}

function test6() {
  rcvdSms = smsList[0];
  log("Test 6: Marking an already unread SMS (id: " + rcvdSms + ") unread.");
  markMessageAndVerify(rcvdSms, false, deleteMsgs);
}

function deleteMsgs() {
  let smsId = smsList.shift();

  log("Deleting SMS (id: " + smsId + ").");
  let request = manager.delete(smsId);
  ok(request instanceof DOMRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      // Message deleted, continue until none are left
      if (smsList.length) {
        deleteMsgs();
      } else {
        cleanUp();
      }
    } else {
      log("SMS delete failed.");
      ok(false, "manager.delete request returned false");
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
