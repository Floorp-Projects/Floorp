/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let sms = window.navigator.mozSms;
let numberMsgs = 10;
let smsList = new Array();

function verifyInitialState() {
  log("Verifying initial state.");
  ok(sms, "mozSms");
  // Ensure test is starting clean with no existing sms messages
  deleteAllMsgs(simulateIncomingSms);
}

function isIn(aVal, aArray, aMsg) {
  ok(aArray.indexOf(aVal) >= 0, aMsg);
}

function deleteAllMsgs(nextFunction) {
  let msgList = new Array();
  let smsFilter = new MozSmsFilter;

  let cursor = sms.getMessages(smsFilter, false);
  ok(cursor instanceof DOMCursor,
      "cursor is instanceof " + cursor.constructor);

  cursor.onsuccess = function(event) {
    // Check if message was found
    if (cursor.result) {
      msgList.push(cursor.result.id);
      // Now get next message in the list
      cursor.continue();
    } else {
      // No (more) messages found
      if (msgList.length) {
        log("Found " + msgList.length + " SMS messages to delete.");
        deleteMsgs(msgList, nextFunction);
      } else {
        log("No SMS messages found.");
        nextFunction();
      }
    }
  };

  cursor.onerror = function(event) {
    log("Received 'onerror' event.");
    ok(event.target.error, "domerror obj");
    log("sms.getMessages error: " + event.target.error.name);
    ok(false,"Could not get SMS messages");
    cleanUp();
  };
}

function deleteMsgs(msgList, nextFunction) {
  let smsId = msgList.shift();

  log("Deleting SMS (id: " + smsId + ").");
  let request = sms.delete(smsId);
  ok(request instanceof DOMRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      // Message deleted, continue until none are left
      if (msgList.length) {
        deleteMsgs(msgList, nextFunction);
      } else {
        log("Finished deleting SMS messages.");
        nextFunction();
      }
    } else {
      log("SMS delete failed.");
      ok(false,"sms.delete request returned false");
      cleanUp();
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "sms.delete request returned unexpected error: "
        + event.target.error.name );
    cleanUp();
  };
}

function simulateIncomingSms() {
  let text = "Incoming SMS number " + (smsList.length + 1);
  let remoteNumber = "5552229797";

  log("Simulating incoming SMS number " + (smsList.length + 1) + " of "
      + numberMsgs + ".");

  // Simulate incoming sms sent from remoteNumber to our emulator
  rcvdEmulatorCallback = false;
  runEmulatorCmd("sms send " + remoteNumber + " " + text, function(result) {
    is(result[0], "OK", "emulator callback");
    rcvdEmulatorCallback = true;
  });
}

// Callback for incoming sms
sms.onreceived = function onreceived(event) {
  log("Received 'onreceived' sms event.");
  let incomingSms = event.message;
  log("Received SMS (id: " + incomingSms.id + ").");

  // Add newly received message to array of received msgs
  smsList.push(incomingSms);

  // Wait for emulator to catch up before continuing
  waitFor(nextRep,function() {
    return(rcvdEmulatorCallback);
  });
};

function nextRep() {
  if (smsList.length < numberMsgs) {
    simulateIncomingSms();
  } else {
    log("Received " + numberMsgs + " sms messages in total.");
    getMsgs(false);
  }
}

function getMsgs(reverse) {
  let smsFilter = new MozSmsFilter;
  let foundSmsCount = 0;
  let foundSmsList = new Array();

  if (!reverse) {
    log("Getting the sms messages.");
  } else {
    log("Getting the sms messages in reverse order.");
  }

  // Note: This test is intended for getMessages, so just a basic test with
  // no filter (default); separate tests will be written for sms filtering
  let cursor = sms.getMessages(smsFilter, reverse);
  ok(cursor instanceof DOMCursor,
      "cursor is instanceof " + cursor.constructor);

  cursor.onsuccess = function(event) {
    log("Received 'onsuccess' event.");

    if (cursor.result) {
      // Another message found
      log("Got SMS (id: " + cursor.result.id + ").");
      foundSmsCount++;
      // Store found message
      foundSmsList.push(cursor.result);
      // Now get next message in the list
      cursor.continue();
    } else {
      // No more messages; ensure correct number found
      if (foundSmsCount == numberMsgs) {
        log("SMS getMessages returned " + foundSmsCount +
            " messages as expected.");  
      } else {
        log("SMS getMessages returned " + foundSmsCount +
            " messages, but expected " + numberMsgs + ".");
        ok(false, "Incorrect number of messages returned by sms.getMessages");
      }
      verifyFoundMsgs(foundSmsList, reverse);
    }
  };

  cursor.onerror = function(event) {
    log("Received 'onerror' event.");
    ok(event.target.error, "domerror obj");
    log("sms.getMessages error: " + event.target.error.name);
    ok(false,"Could not get SMS messages");
    cleanUp();
  };
}

function verifyFoundMsgs(foundSmsList, reverse) {
  if (reverse) {
    smsList.reverse();
  }
  for (var x = 0; x < numberMsgs; x++) {
    is(foundSmsList[x].id, smsList[x].id, "id");
    is(foundSmsList[x].threadId, smsList[x].threadId, "thread id");
    is(foundSmsList[x].body, smsList[x].body, "body");
    is(foundSmsList[x].delivery, smsList[x].delivery, "delivery");
    is(foundSmsList[x].read, smsList[x].read, "read");

    // Bug 805799: receiver null when onreceived event is fired, until do a
    // getMessage. Default emulator (receiver) phone number is 15555215554
    if (!smsList[x].receiver) {
      isIn(foundSmsList[x].receiver, ["15555215554", "+15555215554"], "receiver");
    } else {
      isIn(foundSmsList[x].receiver, [smsList[x].receiver, "+15555215554"], "receiver");
    }

    isIn(foundSmsList[x].sender, [smsList[x].sender, "+15552229797"], "sender");
    is(foundSmsList[x].timestamp.getTime(), smsList[x].timestamp.getTime(),
        "timestamp");
  }

  log("Content in all of the returned SMS messages is correct.");

  if (!reverse) {
    // Now get messages in reverse
    getMsgs(true);
  } else {
    // Finished, delete all messages
    deleteAllMsgs(cleanUp);
  };
}

function cleanUp() {
  sms.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();
