/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 40000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

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
  let request = manager.getMessages(filter, reverse || false);
  request.onsuccess = function(event) {
    if (!request.done) {
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

    let request = manager.delete(message.id);
    request.onsuccess = deleteAll.bind(null, messages);
    request.onerror = function (event) {
      ok(false, "failed to delete all messages");
      tasks.finish();
    }
  });
}

function sendMessage(to, body) {
  manager.onsent = function () {
    manager.onsent = null;
    tasks.next();
  };
  let request = manager.send(to, body);
  request.onerror = tasks.finish.bind(tasks);
}

function receiveMessage(from, body) {
  manager.onreceived = function () {
    manager.onreceived = null;
    tasks.next();
  };
  sendSmsToEmulator(from, body, function (success) {
    if (!success) {
      tasks.finish();
    }
  });
}

function getAllThreads(callback) {
  let threads = [];

  let cursor = manager.getThreads();
  ok(cursor instanceof DOMCursor,
     "cursor is instanceof " + cursor.constructor);

  cursor.onsuccess = function (event) {
    if (!cursor.done) {
      threads.push(cursor.result);
      cursor.continue();
      return;
    }

    window.setTimeout(callback.bind(null, threads), 0);
  };
}

function checkThread(bodies, lastBody, unreadCount, participants,
                     thread, callback) {
  log("Validating MozMobileMessageThread attributes " +
      JSON.stringify([bodies, lastBody, unreadCount, participants]));

  ok(thread, "current thread should be valid.");

  ok(thread.id, "thread id", "thread.id");
  log("Got thread " + thread.id);

  if (lastBody != null) {
    is(thread.body, lastBody, "thread.body");
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

  // Check whether the thread does contain all the messages it supposed to have.
  let filter = new MozSmsFilter;
  filter.threadId = thread.id;
  getAllMessages(function (messages) {
    is(messages.length, bodies.length, "messages.length and bodies.length");

    for (let message of messages) {
      let index = bodies.indexOf(message.body);
      ok(index >= 0, "message.body '" + message.body +
         "' should be found in bodies array.");
      bodies.splice(index, 1);
    }

    is(bodies.length, 0, "bodies array length");

    window.setTimeout(callback, 0);
  }, filter, false);
}

tasks.push(deleteAllMessages);

tasks.push(getAllThreads.bind(null, function (threads) {
  is(threads.length, 0, "Empty thread list at beginning.");
  tasks.next();
}));

// Populate MobileMessageDB with messages.
let checkFuncs = [];

// [Thread 1]
// One message only, body = "thread 1";
// All sent message, unreadCount = 0;
// One participant only, participants = ["5555211001"].
tasks.push(sendMessage.bind(null, "5555211001", "thread 1"));
checkFuncs.push(checkThread.bind(null, ["thread 1"],
                                 "thread 1", 0, ["5555211001"]));

// [Thread 2]
// Two messages, body = "thread 2-2";
// All sent message, unreadCount = 0;
// One participant with two aliased addresses, participants = ["5555211002"].
tasks.push(sendMessage.bind(null, "5555211002",   "thread 2-1"));
tasks.push(sendMessage.bind(null, "+15555211002", "thread 2-2"));
checkFuncs.push(checkThread.bind(null, ["thread 2-1", "thread 2-2"],
                                 "thread 2-2", 0, ["5555211002"]));

// [Thread 3]
// Two messages, body = "thread 3-2";
// All sent message, unreadCount = 0;
// One participant with two aliased addresses, participants = ["+15555211003"].
tasks.push(sendMessage.bind(null, "+15555211003", "thread 3-1"));
tasks.push(sendMessage.bind(null, "5555211003",   "thread 3-2"));
checkFuncs.push(checkThread.bind(null, ["thread 3-1", "thread 3-2"],
                                 "thread 3-2", 0, ["+15555211003"]));

// [Thread 4]
// One message only, body = "thread 4";
// All received message, unreadCount = 1;
// One participant only, participants = ["5555211004"].
tasks.push(receiveMessage.bind(null, "5555211004", "thread 4"));
checkFuncs.push(checkThread.bind(null, ["thread 4"],
                                 "thread 4", 1, ["5555211004"]));

// [Thread 5]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, we're having SMSC time as message
// timestamp and SMSC time resolution is 1 second. So it's likely that the two
// messages have the same timestamp and we just can't tell which is the later
// one.
//
// All received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["5555211005"].
tasks.push(receiveMessage.bind(null, "5555211005",   "thread 5-1"));
tasks.push(receiveMessage.bind(null, "+15555211005", "thread 5-2"));
checkFuncs.push(checkThread.bind(null, ["thread 5-1", "thread 5-2"],
                                 null, 2, ["5555211005"]));

// [Thread 6]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, we're having SMSC time as message
// timestamp and SMSC time resolution is 1 second. So it's likely that the two
// messages have the same timestamp and we just can't tell which is the later
// one.
//
// All received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["+15555211006"].
tasks.push(receiveMessage.bind(null, "+15555211006", "thread 6-1"));
tasks.push(receiveMessage.bind(null, "5555211006",   "thread 6-2"));
checkFuncs.push(checkThread.bind(null, ["thread 6-1", "thread 6-2"],
                                 null, 2, ["+15555211006"]));

// [Thread 7]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, there might be time difference between
// SMSC and device time. So the result of comparing the timestamps of sent and
// received message may not follow the order of requests and may result in
// UNEXPECTED-FAIL in following tests.
//
// Two received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["5555211007"].
tasks.push(sendMessage.bind(null,    "5555211007",   "thread 7-1"));
tasks.push(sendMessage.bind(null,    "+15555211007", "thread 7-2"));
tasks.push(receiveMessage.bind(null, "5555211007",   "thread 7-3"));
tasks.push(receiveMessage.bind(null, "+15555211007", "thread 7-4"));
checkFuncs.push(checkThread.bind(null, ["thread 7-1", "thread 7-2",
                                        "thread 7-3", "thread 7-4"],
                                 null, 2, ["5555211007"]));

// [Thread 8]
//
// Thread body should be set to text body of the last message in this
// thread. However due to BUG 840051, there might be time difference between
// SMSC and device time. So the result of comparing the timestamps of sent and
// received message may not follow the order of requests and may result in
// UNEXPECTED-FAIL in following tests.
//
// Two received message, unreadCount = 2;
// One participant with two aliased addresses, participants = ["5555211008"].
tasks.push(receiveMessage.bind(null, "5555211008",   "thread 8-1"));
tasks.push(receiveMessage.bind(null, "+15555211008", "thread 8-2"));
tasks.push(sendMessage.bind(null,    "5555211008",   "thread 8-3"));
tasks.push(sendMessage.bind(null,    "+15555211008", "thread 8-4"));
checkFuncs.push(checkThread.bind(null, ["thread 8-1", "thread 8-2",
                                        "thread 8-3", "thread 8-4"],
                                 null, 2, ["5555211008"]));

// [Thread 9]
// Three sent message, unreadCount = 0;
// One participant with three aliased addresses, participants = ["+15555211009"].
tasks.push(sendMessage.bind(null, "+15555211009",  "thread 9-1"));
tasks.push(sendMessage.bind(null, "01115555211009", "thread 9-2"));
tasks.push(sendMessage.bind(null, "5555211009",    "thread 9-3"));
checkFuncs.push(checkThread.bind(null, ["thread 9-1", "thread 9-2",
                                        "thread 9-3"],
                                 "thread 9-3", 0, ["+15555211009"]));

// [Thread 10]
// Three sent message, unreadCount = 0;
// One participant with three aliased addresses, participants = ["+15555211010"].
tasks.push(sendMessage.bind(null, "+15555211010",  "thread 10-1"));
tasks.push(sendMessage.bind(null, "5555211010",    "thread 10-2"));
tasks.push(sendMessage.bind(null, "01115555211010", "thread 10-3"));
checkFuncs.push(checkThread.bind(null, ["thread 10-1", "thread 10-2",
                                        "thread 10-3"],
                                 "thread 10-3", 0, ["+15555211010"]));

// [Thread 11]
// Three sent message, unreadCount = 0;
// One participant with three aliased addresses, participants = ["01115555211011"].
tasks.push(sendMessage.bind(null, "01115555211011", "thread 11-1"));
tasks.push(sendMessage.bind(null, "5555211011",    "thread 11-2"));
tasks.push(sendMessage.bind(null, "+15555211011",  "thread 11-3"));
checkFuncs.push(checkThread.bind(null, ["thread 11-1", "thread 11-2",
                                        "thread 11-3"],
                                 "thread 11-3", 0, ["01115555211011"]));

// [Thread 12]
// Three sent message, unreadCount = 0;
// One participant with three aliased addresses, participants = ["01115555211012"].
tasks.push(sendMessage.bind(null, "01115555211012", "thread 12-1"));
tasks.push(sendMessage.bind(null, "+15555211012",  "thread 12-2"));
tasks.push(sendMessage.bind(null, "5555211012",    "thread 12-3"));
checkFuncs.push(checkThread.bind(null, ["thread 12-1", "thread 12-2",
                                        "thread 12-3"],
                                 "thread 12-3", 0, ["01115555211012"]));

// [Thread 13]
// Three sent message, unreadCount = 0;
// One participant with three aliased addresses, participants = ["5555211013"].
tasks.push(sendMessage.bind(null, "5555211013",    "thread 13-1"));
tasks.push(sendMessage.bind(null, "+15555211013",  "thread 13-2"));
tasks.push(sendMessage.bind(null, "01115555211013", "thread 13-3"));
checkFuncs.push(checkThread.bind(null, ["thread 13-1", "thread 13-2",
                                        "thread 13-3"],
                                 "thread 13-3", 0, ["5555211013"]));

// [Thread 14]
// Three sent message, unreadCount = 0;
// One participant with three aliased addresses, participants = ["5555211014"].
tasks.push(sendMessage.bind(null, "5555211014",     "thread 14-1"));
tasks.push(sendMessage.bind(null, "01115555211014", "thread 14-2"));
tasks.push(sendMessage.bind(null, "+15555211014",   "thread 14-3"));
checkFuncs.push(checkThread.bind(null, ["thread 14-1", "thread 14-2",
                                        "thread 14-3"],
                                 "thread 14-3", 0, ["5555211014"]));

// Check threads.
tasks.push(getAllThreads.bind(null, function (threads) {
  is(threads.length, checkFuncs.length, "number of threads got");

  (function callback() {
    if (!threads.length) {
      tasks.next();
      return;
    }

    checkFuncs.shift()(threads.shift(), callback);
  })();
}));

tasks.push(deleteAllMessages);

tasks.push(getAllThreads.bind(null, function (threads) {
  is(threads.length, 0, "Empty thread list at the end.");
  tasks.next();
}));

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
