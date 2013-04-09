/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 40000;

const SELF = 5554;
const NUM_THREADS = 10;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let sms = window.navigator.mozSms;
ok(sms instanceof MozSmsManager);

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
  let request = sms.getMessages(filter, reverse || false);
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

    let request = sms.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function (event) {
      ok(false, "failed to delete all messages");
      tasks.finish();
    }
  });
}

tasks.push(deleteAllMessages);

/**
 * Populate MobileMessageDB with messages to being tests. We'll have NUM_THREADS
 * sent and received messages, and NUM_THREADS/2 unread received messages.
 *
 *   send    to   "0"
 *   receive from "0", count = 1
 *   mark         received as read
 *
 *   send    to   "1"
 *   receive from "1", count = 2
 *
 *   send    to   "2"
 *   receive from "2", count = 3
 *   mark         received as read
 *   ...
 *   send    to   "9"
 *   receive from "9", count = 10
 */
tasks.push(function populateMessages() {
  let count = 0;

  function sendMessage(iter) {
    let request = sms.send("+1555531555" + iter, "Nice to meet you");
    request.onsuccess = function onRequestSuccess(event) {
      sms.addEventListener("received", onReceived);
      sendSmsToEmulator("555541555" + iter, "Nice to meet you, too");
    }
    request.onerror = function onRequestError(event) {
      tasks.finish();
    }
  }

  function onReceived(event) {
    sms.removeEventListener("received", onReceived);

    ++count;
    if (count % 2) {
      let request = sms.markMessageRead(event.message.id, true);
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

tasks.push(function testDeliveryAndNumber() {
  log("Checking delivery == sent && number == 0");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["+15555315550"];
  getAllMessages(function (messages) {
    // Only { delivery: "sent", receiver: "+15555315550", read: true }
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    getAllMessages(function (messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testDeliveryAndNumberNotFound() {
  log("Checking delivery == sent && number == 12345");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["12345"];
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testDeliveryAndRead() {
  log("Checking delivery == received && read == true");
  let filter = new MozSmsFilter();
  filter.delivery = "received";
  filter.read = true;
  getAllMessages(function (messages) {
    // { delivery: "received", sender: "5555415550", read: true },
    // { delivery: "received", sender: "5555415552", read: true },
    // { delivery: "received", sender: "5555415554", read: true },
    // { delivery: "received", sender: "5555415556", read: true }, and
    // { delivery: "received", sender: "5555415558", read: true },
    is(messages.length, NUM_THREADS / 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      is(message.read, filter.read, "message read");
    }

    getAllMessages(function (messages_r) {
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
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testNumberAndRead() {
  log("Checking number == 0 && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555415550"];
  filter.read = true;
  getAllMessages(function (messages) {
    // { delivery: "received", sender: "5555415550", read: true }
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0]))) {
        ok(false, "message sendor or receiver number");
      }
      is(message.read, filter.read, "message read");
    }

    getAllMessages(function (messages_r) {
      is(messages.length, messages_r.length, "message count");
      for (let i = 0; i < messages_r.length; i++) {
        is(messages_r[i].id, messages[messages.length - 1 - i].id, "message id");
      }

      tasks.next();
    }, filter, true);
  }, filter);
});

tasks.push(function testNumberAndReadNotFound() {
  log("Checking number == 12345 && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["12345"];
  filter.read = true;
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbers() {
  log("Checking number == 0 || number == 1");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555415550", "5555415551"];
  getAllMessages(function (messages) {
    // { delivery: "received", sender: "5555415550", read: true }
    // { delivery: "received", sender: "5555415551", read: false }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0])
            || (message.sender == filter.numbers[1])
            || (message.receiver == filter.numbers[1]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbersNotFound() {
  log("Checking number == 12345 || number == 6789");
  let filter = new MozSmsFilter();
  filter.numbers = ["12345", "6789"];
  getAllMessages(function (messages) {
    is(messages.length, 0, "message count");

    tasks.next();
  }, filter);
});

tasks.push(function testDeliveryAndMultipleNumbers() {
  log("Checking delivery == sent && (number == 0 || number == 1)");
  let filter = new MozSmsFilter();
  filter.delivery = "sent";
  filter.numbers = ["+15555315550", "+15555315551"];
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "+15555315550", read: true }
    // { delivery: "sent", receiver: "+15555315551", read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.delivery, filter.delivery, "message delivery");
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0])
            || (message.sender == filter.numbers[1])
            || (message.receiver == filter.numbers[1]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testMultipleNumbersAndRead() {
  log("Checking (number == 0 || number == 1) && read == true");
  let filter = new MozSmsFilter();
  filter.numbers = ["+15555315550", "5555415550"];
  filter.read = true;
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "+15555315550", read: true }
    // { delivery: "received", sender: "5555415550", read: true }
    is(messages.length, 2, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      is(message.read, filter.read, "message read");
      if (!((message.sender == filter.numbers[0])
            || (message.receiver == filter.numbers[0])
            || (message.sender == filter.numbers[1])
            || (message.receiver == filter.numbers[1]))) {
        ok(false, "message sendor or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testNationalNumber() {
  log("Checking number = 5555315550");
  let filter = new MozSmsFilter();
  filter.numbers = ["5555315550"];
  getAllMessages(function (messages) {
    // { delivery: "sent", receiver: "+15555315550", read: true }
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!((message.sender == "+15555315550")
            || (message.receiver == "+15555315550"))) {
        ok(false, "message sendor or receiver number");
      }
    }

    tasks.next();
  }, filter);
});

tasks.push(function testInternationalNumber() {
  log("Checking number = +15555415550");
  let filter = new MozSmsFilter();
  filter.numbers = ["+15555415550"];
  getAllMessages(function (messages) {
    // { delivery: "received", sender: "5555415550", read: true }
    is(messages.length, 1, "message count");
    for (let i = 0; i < messages.length; i++) {
      let message = messages[i];
      if (!((message.sender == "5555415550")
            || (message.receiver == "5555415550"))) {
        ok(false, "message sendor or receiver number");
      }
    }

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
