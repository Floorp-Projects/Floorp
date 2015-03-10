/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests queue module's clear and getMessageCount functions work.
 */

add_task(function*() {
  let { promise, resolve } = Promise.defer();
  let { Queue } = devtools.require("devtools/webaudioeditor/queue");

  let q = new Queue();
  let messages = [];
  let cleared = false;
  let lastProcessedId = null;

  let processed = Task.async(function *(id) {
    if (cleared) {
      throw new Error("Messages should not be processed after clearing the queue");
    }
    messages.push(id);

    is(q.getMessageCount(), 10 - messages.length, "getMessageCount() should count all queued up messages");

    // On our 5th message, clear out all remaining tasks
    if (messages.length === 5) {
      is(lastProcessedId, 3, "Current message is not yet finished processing");
      yield q.clear();
      is(lastProcessedId, 4, "Current message is finished processing after yielding from queue.clear()");
      cleared = true;

      is(q.getMessageCount(), 0, "getMessageCount() returns 0 after being cleared");
      // Wait a bit before finishing to ensure no more messages get processed
      setTimeout(resolve, 300);
    }
  });

  let exec = q.addHandler(function (id) {
    return Task.spawn(function () {
      yield wait(1);
      processed(id);
    }).then(() => lastProcessedId = id);
  });

  for (let i = 0; i < 10; i++) {
    exec(i);
  }

  return promise;
});
