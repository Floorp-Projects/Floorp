/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

const SENDER = "5555552368"; // the remote number
const RECEIVER = "15555215554"; // the emulator's number

let manager = window.navigator.mozMobileMessage;
let MSG_TEXT = "Mozilla Firefox OS!";
let SMS_NUMBER = 100;

let SmsList = [];
let checkDone = true;
let emulatorReady = true;

let pendingEmulatorCmdCount = 0;
function sendSmsToEmulator(from, text) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms send " + from + " " + text;
  runEmulatorCmd(cmd, function(result) {
    --pendingEmulatorCmdCount;

    is(result[0], "OK", "Emulator response");
  });
}

let tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function(func) {
    this._tasks.push(func);
  },

  next: function() {
    let index = this._nextTaskIndex++;
    let task = this._tasks[index];
    try {
      task();
    } catch (ex) {
      ok(false, "test task[" + index + "] throws: " + ex);
      // Run last task as clean up if possible.
      if (index != this._tasks.length - 1) {
        this.finish();
      }
    }
  },

  finish: function() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function() {
    this.next();
  }
};

function taskNextWrapper() {
  tasks.next();
}

function verifySmsExists(incomingSms) {
  log("Getting SMS (id: " + incomingSms.id + ").");
  let requestRet = manager.getMessage(incomingSms.id);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, incomingSms.id, "found SMS id matches");
    is(foundSms.threadId, incomingSms.threadId, "found SMS thread id matches");
    is(foundSms.body, MSG_TEXT, "found SMS msg text matches");
    is(foundSms.delivery, "received", "delivery");
    is(foundSms.deliveryStatus, "success", "deliveryStatus");
    is(foundSms.read, false, "read");
    is(foundSms.receiver, RECEIVER, "receiver");
    is(foundSms.sender, SENDER, "sender");
    is(foundSms.messageClass, "normal", "messageClass");
    log("Got SMS (id: " + foundSms.id + ") as expected.");

    SmsList.push(incomingSms);
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + incomingSms.id + ") but should have.");
    ok(false,"SMS was not found");
    tasks.finish();
  };
}

let verifDeletedCount = 0;
function verifySmsDeleted(smsId) {
  log("Getting SMS (id: " + smsId + ").");
  let requestRet = manager.getMessage(smsId);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, smsId, "found SMS id matches");
    is(foundSms.body, MSG_TEXT, "found SMS msg text matches");
    log("Got SMS (id: " + foundSms.id + ") but should not have.");
    ok(false, "SMS was not deleted");
    tasks.finish();
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + smsId + ") as expected.");
    verifDeletedCount++;
  };
}

tasks.push(function init() {
  log("Initialize test object.");
  ok(manager instanceof MozMobileMessageManager,
     "manager is instance of " + manager.constructor);

  // Callback for incoming sms
  manager.onreceived = function onreceived(event) {
    log("Received 'onreceived' event.");
    let incomingSms = event.message;
    ok(incomingSms, "incoming sms");
    ok(incomingSms.id, "sms id");
    log("Received SMS (id: " + incomingSms.id + ").");
    ok(incomingSms.threadId, "thread id");
    is(incomingSms.body, MSG_TEXT, "msg body");
    is(incomingSms.delivery, "received", "delivery");
    is(incomingSms.deliveryStatus, "success", "deliveryStatus");
    is(incomingSms.read, false, "read");
    is(incomingSms.receiver, RECEIVER, "receiver");
    is(incomingSms.sender, SENDER, "sender");
    is(incomingSms.messageClass, "normal", "messageClass");
    is(incomingSms.deliveryTimestamp, 0, "deliveryTimestamp is 0");

    verifySmsExists(incomingSms);
  };

  tasks.next();
});

tasks.push(function sendAllSms() {
  log("Send " + SMS_NUMBER + " SMS");
  for (let i = 0; i < SMS_NUMBER; i++) {
    sendSmsToEmulator(SENDER, MSG_TEXT);
  }

  waitFor(taskNextWrapper, function() {
    return (pendingEmulatorCmdCount === 0) && (SmsList.length === SMS_NUMBER);
  });
});

tasks.push(function deleteAllSms() {
  log("Deleting SMS using smsmsg obj array parameter.");
  let deleteStart = Date.now();
  log("deleteStart: " + deleteStart);
  log("SmsList: " + JSON.stringify(SmsList));
  let requestRet = manager.delete(SmsList);
  ok(requestRet,"smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    let deleteDone = Date.now();
    log("Delete " + SMS_NUMBER + " SMS takes " + (deleteDone - deleteStart) + " ms.");
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      for (let i = 0; i < SmsList.length; i++) {
        verifySmsDeleted(SmsList[i].id);
      }
    } else {
      log("smsrequest returned false for manager.delete");
      ok(false, "SMS delete failed");
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.delete request returned unexpected error: "
        + event.target.error.name);
    tasks.finish();
  };

  waitFor(taskNextWrapper, function() {
    return verifDeletedCount === SMS_NUMBER;
  });
});

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  manager.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.setBoolPref("dom.sms.enabled", false);
  log("Finish!!!");
  finish();
});

// Start the test
tasks.run();
