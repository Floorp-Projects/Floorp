/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const FROM = "09123456789";
const PREFIX = "msg_";
const MSGS = 3;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

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
  let cursor = manager.getMessages(filter, reverse || false);
  cursor.onsuccess = function(event) {
    if (!this.done) {
      messages.push(this.result);
      this.continue();
      return;
    }

    window.setTimeout(callback.bind(null, messages), 0);
  }
}

function getAllThreads(callback) {
  let threads = [];
  let cursor = manager.getThreads();
  cursor.onsuccess = function(event) {
    if (!this.done) {
      threads.push(this.result);
      this.continue();
      return;
    }

    window.setTimeout(callback.bind(null, threads), 0);
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

function checkThread(thread, id, body, unreadCount, timestamp)
{
  is(thread.id, id, "Thread ID is set");
  is(thread.body, body, "Thread subject is set to last message body");
  is(thread.unreadCount, unreadCount, "Thread unread count");
  is(JSON.stringify(thread.participants), JSON.stringify([FROM]), "Thread participants");
  is(thread.timestamp.getTime(), timestamp.getTime(), "Thread timestamp is set");
}

tasks.push(deleteAllMessages);

tasks.push(getAllThreads.bind(null, function (threads) {
  is(threads.length, 0, "Threads are cleared");
  tasks.next();
}));

let gotMessagesCount = 0;
tasks.push(function () {
  manager.onreceived = function () {
    ++gotMessagesCount;
  };
  tasks.next();
});

tasks.push(function () {
  for (let i = 1; i <= MSGS; i++) {
    sendSmsToEmulator(FROM, PREFIX + i);
  }
  tasks.next();
});

let allMessages;
tasks.push(function waitAllMessageReceived() {
  if (gotMessagesCount != MSGS) {
    window.setTimeout(waitAllMessageReceived, 100);
    return;
  }

  getAllMessages(function (messages) {
    allMessages = messages;
    tasks.next();
  });
});


let originalThread;
tasks.push(getAllThreads.bind(null, function (threads) {
  is(threads.length, 1, "Should have only one thread");

  let lastMessage = allMessages[allMessages.length - 1];

  originalThread = threads[0];
  checkThread(originalThread,
              originalThread.id,
              lastMessage.body,
              allMessages.length,
              lastMessage.timestamp);

  tasks.next();
}));

tasks.push(function DeleteFirstMessage() {
  let request = manager.delete(allMessages[0].id);
  request.onsuccess = function () {
    getAllThreads(function (threads) {
      is(threads.length, 1, "Should have only one thread");

      allMessages = allMessages.slice(1);
      let lastMessage = allMessages[allMessages.length - 1];

      checkThread(threads[0],
                  originalThread.id,
                  lastMessage.body,
                  allMessages.length,
                  lastMessage.timestamp);

      tasks.next();
    });
  };
});

tasks.push(function DeleteLastMessage() {
  let request = manager.delete(allMessages[allMessages.length - 1].id);
  request.onsuccess = function () {
    getAllThreads(function (threads) {
      is(threads.length, 1, "Should have only one thread");

      allMessages = allMessages.slice(0, -1);
      let lastMessage = allMessages[allMessages.length - 1];

      checkThread(threads[0],
                  originalThread.id,
                  lastMessage.body,
                  allMessages.length,
                  lastMessage.timestamp);

      tasks.next();
    });
  };
});

tasks.push(deleteAllMessages);

tasks.push(getAllThreads.bind(null, function (threads) {
  is(threads.length, 0, "Should have deleted all threads");
  tasks.next();
}));

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  manager.onreceived = null;

  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
});

tasks.run();
