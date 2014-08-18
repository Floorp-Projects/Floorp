/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const SELF = 5554;
const NUM_THREADS = 10;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

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

function deleteAllMessages(next) {
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

function checkSenderOrReceiver(message, number) {
  return message.sender == number ||
         message.sender == ("+1" + number) ||
         message.receiver == number ||
         message.receiver == ("+1" + number);
}

tasks.push(deleteAllMessages);

/**
 * Populate MobileMessageDB with messages to being tests. We'll have NUM_THREADS
 * sent and received messages, and NUM_THREADS/2 unread received messages.
 *
 *   send    to   "+15555315550"
 *   receive from "5555315550", count = 1
 *   mark         received as read
 *
 *   send    to   "+15555315551"
 *   receive from "5555315551", count = 2
 *
 *   send    to   "+15555315552"
 *   receive from "5555315552", count = 3
 *   mark         received as read
 *   ...
 *   send    to   "+15555315559"
 *   receive from "5555315559", count = 10
 */
let threadIds = [];
tasks.push(function populateMessages() {
  let count = 0;

  function sendMessage(iter) {
    let request = manager.send("+1555531555" + iter, "Nice to meet you");
    request.onsuccess = function onRequestSuccess(event) {
      manager.addEventListener("received", onReceived);

      threadIds.push(request.result.threadId);

      sendSmsToEmulator("555531555" + iter, "Nice to meet you, too");
    }
    request.onerror = function onRequestError(event) {
      tasks.finish();
    }
  }

  function onReceived(event) {
    manager.removeEventListener("received", onReceived);

    if (event.message.threadId != threadIds[threadIds.length - 1]) {
      ok(false, "Thread IDs of sent and received message mismatch.");
      tasks.finish();
      return;
    }

    ++count;
    if (count % 2) {
      let request = manager.markMessageRead(event.message.id, true);
      request.onsuccess = function onRequestSuccess(event) {
        if (count < NUM_THREADS) {
          sendMessage(count);
        } else {
          tasks.next();
        }
      }
      request.onerror = function onRequestError(event) {
        tasks.finish();
      }
    } else if (count < NUM_THREADS) {
      sendMessage(count);
    } else {
      tasks.next();
    }
  }

  sendMessage(count);
});

let INVALID_NUMBER = "12345";
let INVALID_NUMBER2 = "6789";

let INVALID_THREAD_ID;
tasks.push(function assignInvalidThreadID() {
  INVALID_THREAD_ID = threadIds[threadIds.length - 1] + 1;
  log("Set INVALID_THREAD_ID to be " + INVALID_THREAD_ID);
  tasks.next();
});

tasks.push(function testDeliveryAndNumber() {
  log("Checking delivery == sent && number == 5555315550");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["5555315550"];
  getAllMessages(function(messages) {
    // Only { delivery: "sent", receiver: "+15555315550", read: true }
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      if (!checkSenderOrReceiver(message, filter.numbers[0])) {
        ok(false, "message sender or receiver number");
      }
    }

    getAllMessages(function(messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testDeliveryAndNumberNotFound() {
  log("Checking delivery == sent && number == INVALID_NUMBER");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = [INVALID_NUMBER];
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testDeliveryAndRead() {
  log("Checking delivery == received && read == true");
  let filter = new MozSmsFilter();
  filter.delivery = "received";
  filter.read = true;
  getAllMessages(function(messages) {
    // { delivery: "received", sender: "5555315550", read: true },
    // { delivery: "received", sender: "5555315552", read: true },
    // { delivery: "received", sender: "5555315554", read: true },
    // { delivery: "received", sender: "5555315556", read: true },
    // { delivery: "received", sender: "5555315558", read: true },
    is(messages.length, NUM_THREADS / 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      is(message.read, filter.read, "message read");
    }

    getAllMessages(function(messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testDeliveryAndReadNotFound() {
  log("Checking delivery == sent && read == false");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.read = false;
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testDeliveryAndThreadId() {
  log("Checking delivery == received && threadId == " + threadIds[0]);
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.threadId = threadIds[0];
  getAllMessages(function(messages) {
    // { delivery: "sent", receiver: "+15555315550", threadId: threadIds[0]}
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      is(message.threadId, filter.threadId, "message threadId");
    }

    getAllMessages(function(messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testDeliveryAndThreadIdNotFound() {
  log("Checking delivery == sent && threadId == INVALID_THREAD_ID");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.threadId = INVALID_THREAD_ID;
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testNumberAndRead() {
  log("Checking number == 5555315550 && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550"];
  filter.read = true;
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!checkSenderOrReceiver(message, filter.numbers[0])) {
        ok(false, "message sender or receiver number");
      }
      is(message.read, filter.read, "message read");
    }

    getAllMessages(function(messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testNumberAndReadNotFound() {
  log("Checking number == INVALID_NUMBER && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = [INVALID_NUMBER];
  filter.read = true;
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testNumberAndThreadId() {
  log("Checking number == 5555315550 && threadId == " + threadIds[0]);
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550"];
  filter.threadId = threadIds[0];
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!checkSenderOrReceiver(message, filter.numbers[0])) {
        ok(false, "message sender or receiver number");
      }
      is(message.threadId, filter.threadId, "message threadId");
    }

    getAllMessages(function(messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testNumberAndThreadIdNotFound() {
  log("Checking number == INVALID_NUMBER && threadId == INVALID_THREAD_ID");
  let filter = new MozSmsFilter();
  filter.numbers = [INVALID_NUMBER];
  filter.threadId = INVALID_THREAD_ID;
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbers() {
  log("Checking number == 5555315550 || number == 5555315551");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550", "5555315551"];
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    // { delivery: "sent",     receiver: "+15555315551", read: true }
    // { delivery: "received", sender: "5555315551",     read: false }
    is(messages.length, 4, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!(checkSenderOrReceiver(message, filter.numbers[0]) ||
            checkSenderOrReceiver(message, filter.numbers[1]))) {
        ok(false, "message sender or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbersNotFound() {
  log("Checking number == INVALID_NUMBER || number == INVALID_NUMBER2");
  let filter = new MozSmsFilter();
  filter.numbers = [INVALID_NUMBER, INVALID_NUMBER2];
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testDeliveryAndMultipleNumbers() {
  log("Checking delivery == sent && (number == 5555315550 || number == 5555315551)");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["5555315550", "5555315551"];
  getAllMessages(function(messages) {
    // { delivery: "sent", receiver: "+15555315550", read: true }
    // { delivery: "sent", receiver: "+15555315551", read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      if (!(checkSenderOrReceiver(message, filter.numbers[0]) ||
            checkSenderOrReceiver(message, filter.numbers[1]))) {
        ok(false, "message sender or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbersAndRead() {
  log("Checking (number == 5555315550 || number == 5555315551) && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550", "5555315551"];
  filter.read = true;
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    // { delivery: "sent",     receiver: "+15555315551", read: true }
    is(messages.length, 3, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.read, filter.read, "message read");
      if (!(checkSenderOrReceiver(message, filter.numbers[0]) ||
            checkSenderOrReceiver(message, filter.numbers[1]))) {
        ok(false, "message sender or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbersAndThreadId() {
  log("Checking (number == 5555315550 || number == 5555315551) && threadId == " + threadIds[0]);
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550", "5555315551"];
  filter.threadId = threadIds[0];
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.threadId, filter.threadId, "message threadId");
      if (!(checkSenderOrReceiver(message, filter.numbers[0]) ||
            checkSenderOrReceiver(message, filter.numbers[1]))) {
        ok(false, "message sender or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testNationalNumber() {
  log("Checking number = 5555315550");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550"];
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!checkSenderOrReceiver(message, filter.numbers[0])) {
        ok(false, "message sender or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testInternationalNumber() {
  log("Checking number = +15555315550");
  let filter = new MozSmsFilter();
  filter.numbers = ["+15555315550"];
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!checkSenderOrReceiver(message, "5555315550")) {
        ok(false, "message sender or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testReadAndThreadId() {
  log("Checking read == true && threadId == " + threadIds[0]);
  let filter = new MozSmsFilter();
  filter.read = true;
  filter.threadId = threadIds[0];
  getAllMessages(function(messages) {
    // { delivery: "sent",     receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555315550",     read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.read, filter.read, "message read");
      is(message.threadId, filter.threadId, "message threadId");
    }

    getAllMessages(function(messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testReadAndThreadIdNotFound() {
  log("Checking read == true && threadId == INVALID_THREAD_ID");
  let filter = new MozSmsFilter();
  filter.read = true;
  filter.threadId = INVALID_THREAD_ID;
  getAllMessages(function(messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

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
