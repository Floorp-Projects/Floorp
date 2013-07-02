/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

let pendingEmulatorCmdCount = 0;
function sendSmsToEmulator(from, text) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms send " + from + " " + text;
  runEmulatorCmd(cmd, function (result) {
    --pendingEmulatorCmdCount;

    is(result[0], "OK", "Emulator response");
  });
}

let tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function push(func) {
    this._tasks.push(func);
  },

  next: function next() {
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

  finish: function finish() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function run() {
    this.next();
  }
};

function getAllMessages(callback, filter, reverse) {
  if (!filter) {
    filter = new MozSmsFilter;
  }
  let messages = [];
  let request = mozSms.getMessages(filter, reverse || false);
  request.onsuccess = function(event) {
    if (request.result) {
      messages.push(request.result);
      request.continue();
      return;
    }

    window.setTimeout(callback.bind(null, messages), 0);
  }
}

function deleteAllMessages() {
  getAllMessages(function deleteAll(messages) {
    let message = messages.shift();
    if (!message) {
      ok(true, "all messages deleted");
      tasks.next();
      return;
    }

    let request = mozSms.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function (event) {
      ok(false, "failed to delete all messages");
      tasks.finish();
    }
  });
}

function validate(number, normalizedNumber) {
  log("Checking ('" + number + "', '" + normalizedNumber + "')");

  let sendRequest = mozSms.send(number, "ping");
  sendRequest.onsuccess = function onSendSuccess(event) {
    let sentMessage = event.target.result;

    mozSms.onreceived = function onreceived(event) {
      let receivedMessage = event.message;
      is(sentMessage.threadId, receivedMessage.threadId,
         "message threadIds are supposed to be matched");

      tasks.next();
    };
    sendSmsToEmulator(normalizedNumber, "pong");
  };
  sendRequest.onerror = function onSendError(event) {
    ok(false, "failed to send message.");
    tasks.finish();
  };
}

let mozSms = window.navigator.mozSms;
tasks.push(function () {
  log("Checking mozSms.");
  ok(mozSms instanceof MozSmsManager);
  tasks.next();
});

tasks.push(deleteAllMessages);

tasks.push(validate.bind(null, "+886-9-87-654-321", "+886987654321"));
tasks.push(validate.bind(null, "+886 9 87654321", "+886987654321"));
tasks.push(validate.bind(null, "+886(9)87654321", "+886987654321"));

tasks.push(deleteAllMessages);

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
});

tasks.run();
