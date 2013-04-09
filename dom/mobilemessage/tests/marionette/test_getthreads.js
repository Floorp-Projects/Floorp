/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 40000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let sms = window.navigator.mozSms;
ok(sms instanceof MozSmsManager);

let pendingEmulatorCmdCount = 0;
function sendSmsToEmulator(from, text, callback) {
  ++pendingEmulatorCmdCount;

  let cmd = "sms send " + from + " " + text;
  runEmulatorCmd(cmd, function (result) {
    --pendingEmulatorCmdCount;

    callback(result[0] == "OK");
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
      task.apply(null, Array.slice(arguments));
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

function deleteAllMessages() {
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

function sendMessage(to, body) {
  sms.onsent = function () {
    sms.onsent = null;
    tasks.next();
  };
  let request = sms.send(to, body);
  request.onerror = tasks.finish.bind(tasks);
}

function receiveMessage(from, body) {
  sms.onreceived = function () {
    sms.onreceived = null;
    tasks.next();
  };
  sendSmsToEmulator(from, body, function (success) {
    if (!success) {
      tasks.finish();
    }
  });
}

function checkThread(body, unreadCount, participants, cursor) {
  log("Validating MozMobileMessageThread attributes ...");

  ok(!cursor.done, "cursor session is not yet done");
  let thread = cursor.result;
  ok(thread, "current thread should be valid.");

  ok(thread.id, "thread id", "thread.id");
  log("Got thread " + thread.id);

  if (body != null) {
    is(thread.body, body, "thread.body");
  }

  is(thread.unreadCount, unreadCount, "thread.unreadCount");

  ok(Array.isArray(thread.participants), "thread.participants is array");
  is(thread.participants.length, participants.length,
     "thread.participants.length");
  for (let i = 0; i < participants.length; i++) {
    is(thread.participants[i], participants[i],
       "thread.participants[" + i + "]");
  }

  ok(thread.timestamp instanceof Date, "thread.timestamp");

  cursor.continue();
}

tasks.push(deleteAllMessages);

tasks.push(function () {
  let cursor = sms.getThreads();
  ok(cursor instanceof DOMCursor,
     "cursor is instanceof " + cursor.constructor);

  cursor.onsuccess = function (event) {
    ok(cursor.done, "Empty thread list at beginning.");
    tasks.next();
  };
  cursor.onerror = tasks.finish.bind(tasks);
});

/**
 * Populate MobileMessageDB with messages.
 */
tasks.push(sendMessage.bind(null,    "5555211000",   "thread 1"));

tasks.push(sendMessage.bind(null,    "5555211001",   "thread 2-1"));
tasks.push(sendMessage.bind(null,    "+15555211001", "thread 2-2"));

tasks.push(sendMessage.bind(null,    "+15555211002", "thread 3-1"));
tasks.push(sendMessage.bind(null,    "5555211002",   "thread 3-2"));

tasks.push(receiveMessage.bind(null, "5555211003",   "thread 4"));

tasks.push(receiveMessage.bind(null, "5555211004",   "thread 5-1"));
tasks.push(receiveMessage.bind(null, "+15555211004", "thread 5-2"));

tasks.push(receiveMessage.bind(null, "+15555211005", "thread 6-1"));
tasks.push(receiveMessage.bind(null, "5555211005",   "thread 6-2"));

tasks.push(sendMessage.bind(null,    "5555211006",   "thread 7-1"));
tasks.push(sendMessage.bind(null,    "+15555211006", "thread 7-2"));
tasks.push(receiveMessage.bind(null, "5555211006",   "thread 7-3"));
tasks.push(receiveMessage.bind(null, "+15555211006", "thread 7-4"));

tasks.push(receiveMessage.bind(null, "5555211007",   "thread 8-1"));
tasks.push(receiveMessage.bind(null, "+15555211007", "thread 8-2"));
tasks.push(sendMessage.bind(null,    "5555211007",   "thread 8-3"));
tasks.push(sendMessage.bind(null,    "+15555211007", "thread 8-4"));

tasks.push(function initThreadCursor() {
  let cursor = sms.getThreads();
  ok(cursor instanceof DOMCursor,
     "cursor is instanceof " + cursor.constructor);

  cursor.onsuccess = function (event) {
    tasks.next(cursor);
  };
  cursor.onerror = tasks.finish.bind(tasks);
})

// [Thread 1]
// One message only, body = "thread 1";
// All sent message, unreadCount = 0;
// One participant only, participants = ["5555211000"].
tasks.push(checkThread.bind(null, "thread 1", 0, ["5555211000"]));

// [Thread 2]
// Two messages, body = "thread 2-2";
// All sent message, unreadCount = 0;
// One participant with two aliased addresses, participants = ["5555211001"].
tasks.push(checkThread.bind(null, "thread 2-2", 0, ["5555211001"]));

// [Thread 3]
// Two messages, body = "thread 3-2";
// All sent message, unreadCount = 0;
// One participant with two aliased addresses, participants = ["+15555211002"].
tasks.push(checkThread.bind(null, "thread 3-2", 0, ["+15555211002"]));

// [Thread 4]
// One message only, body = "thread 4";
// All received message, unreadCount = 1;
// One participant only, participants = ["5555211000"].
tasks.push(checkThread.bind(null, "thread 4", 1, ["5555211003"]));

// [Thread 5]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, we're having SMSC time as message
// timestamp and SMSC time resolution is 1 second. So it's likely that the two
// messages have the same timestamp and we just can't tell which is the later
// one.
//
// All received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["5555211004"].
tasks.push(checkThread.bind(null, null, 2, ["5555211004"]));

// [Thread 6]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, we're having SMSC time as message
// timestamp and SMSC time resolution is 1 second. So it's likely that the two
// messages have the same timestamp and we just can't tell which is the later
// one.
//
// All received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["+15555211005"].
tasks.push(checkThread.bind(null, null, 2, ["+15555211005"]));

// [Thread 7]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, there might be time difference between
// SMSC and device time. So the result of comparing the timestamps of sent and
// received message may not follow the order of requests and may result in
// UNEXPECTED-FAIL in following tests.
//
// Two received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["5555211006"].
tasks.push(checkThread.bind(null, null, 2, ["5555211006"]));

// [Thread 8]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, there might be time difference between
// SMSC and device time. So the result of comparing the timestamps of sent and
// received message may not follow the order of requests and may result in
// UNEXPECTED-FAIL in following tests.
//
// Two received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["5555211007"].
tasks.push(checkThread.bind(null, null, 2, ["5555211007"]));

tasks.push(function checkThreadCursorEnds(cursor) {
  log("Validating whether cursor session ends");

  ok(cursor.done, "cursor session is done");
  ok(cursor.result == null, "cursor result should be reset now.");

  tasks.next();
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
