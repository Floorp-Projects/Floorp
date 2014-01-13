/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const NUM_THREADS = 10;
const REMOTE_NATIONAL_NUMBER = "555531555";
const REMOTE_INTERNATIONAL_NUMBER = "+1" + REMOTE_NATIONAL_NUMBER;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

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

let manager;
function getAllMessages(callback, filter, reverse) {
  if (!filter) {
    filter = new MozSmsFilter;
  }
  let messages = [];
  let request = manager.getMessages(filter, reverse || false);
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
  log("Deleting all messages.");
  getAllMessages(function deleteAll(messages) {
    let message = messages.shift();
    if (!message) {
      ok(true, "all messages deleted");
      tasks.next();
      return;
    }

    let request = manager.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function(event) {
      ok(false, "failed to delete all messages");
      tasks.finish();
    }
  });
}

function checkMessage(needle, secondary) {
  log("  Verifying " + needle);

  let filter = new MozSmsFilter();
  filter.numbers = [needle];
  getAllMessages(function(messages) {
    is(messages.length, 2, "should have exactly 2 messages");

    // Check the messages are sent to/received from either 'needle' or
    // 'secondary' number.
    let validNumbers = [needle, secondary];
    for (let message of messages) {
      let number = (message.delivery === "received") ? message.sender
                                                     : message.receiver;
      let index = validNumbers.indexOf(number);
      ok(index >= 0, "message.number");
      validNumbers.splice(index, 1); // Remove from validNumbers.
    }

    tasks.next();
  }, filter);
}

tasks.push(function verifyInitialState() {
  log("Verifying initial state.");
  manager = window.navigator.mozMobileMessage;
  ok(manager instanceof MozMobileMessageManager,
     "manager is instance of " + manager.constructor);
  tasks.next();
});

tasks.push(deleteAllMessages);

/**
 * Populate database with messages to being tests. We'll have NUM_THREADS
 * sent and received messages.
 *
 *   send    to   "+15555315550"
 *   receive from "5555315550", count = 1
 *
 *   send    to   "+15555315551"
 *   receive from "5555315551", count = 2
 *   ...
 *   send    to   "+15555315559"
 *   receive from "5555315559", count = 10
 */
tasks.push(function populateMessages() {
  log("Populating messages.");
  let count = 0;

  function sendMessage(iter) {
    let request = manager.send(REMOTE_INTERNATIONAL_NUMBER + iter,
                               "Nice to meet you");
    request.onsuccess = function onRequestSuccess(event) {
      sendSmsToEmulator(REMOTE_NATIONAL_NUMBER + iter,
                        "Nice to meet you, too");
    }
    request.onerror = function onRequestError(event) {
      tasks.finish();
    }
  }

  manager.addEventListener("received", function onReceived(event) {
    ++count;
    if (count < NUM_THREADS) {
      sendMessage(count);
    } else {
      manager.removeEventListener("received", onReceived);
      tasks.next();
    }
  });

  sendMessage(count);
});

tasks.push(function() {
  log("Verifying number of messages in database");
  getAllMessages(function(messages) {
    is(messages.length, NUM_THREADS * 2,
       "should have exactly " + (NUM_THREADS * 2) + " messages");

    tasks.next();
  });
});

for (let iter = 0; iter < NUM_THREADS; iter++) {
  let national = REMOTE_NATIONAL_NUMBER + iter;
  let international = REMOTE_INTERNATIONAL_NUMBER + iter;
  tasks.push(checkMessage.bind(null, national, international));
  tasks.push(checkMessage.bind(null, international, national));
}

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
