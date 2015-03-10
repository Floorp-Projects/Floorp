/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure the queue module created for bug 1141261 works as intended
 */

add_task(function*() {
  let { promise, resolve } = Promise.defer();
  let { Queue } = devtools.require("devtools/webaudioeditor/queue");

  let q = new Queue();
  let messages = [];

  let create = q.addHandler(function (id) {
    return Task.spawn(function () {
      yield wait(1);
      processed("create", id);
    });
  });

  let connect = q.addHandler(function (source, dest) {
    processed("connect", source, dest);
  });

  create(1);
  create(2);
  connect(1, 2);
  create(5);
  connect(1, 5);
  create(6);


  function processed () {
    messages.push(arguments);

    // If we have 6 messages, check the order
    if (messages.length === 6) {
      checkFirstBatch();

      // Fire off more messages after the queue is done,
      // waiting a tick to ensure it works after draining
      // the queue
      Task.spawn(function () {
        create(10);
        connect(1, 10);
      });
      return;
    }

    if (messages.length === 8) {
      checkSecondBatch();
      resolve();
    }
  }

  function checkFirstBatch () {
    is(messages[0][0], "create", "first message check");
    is(messages[0][1], 1, "first message args");
    is(messages[1][0], "create", "second message check");
    is(messages[1][1], 2, "second message args");
    is(messages[2][0], "connect", "third message check (sync)");
    is(messages[2][1], 1, "third message args");
    is(messages[2][2], 2, "third message args");
    is(messages[3][0], "create", "fourth message check");
    is(messages[3][1], 5, "fourth message args");
    is(messages[4][0], "connect", "fifth message check (sync)");
    is(messages[4][1], 1, "fifth message args");
    is(messages[4][2], 5, "fifth message args");
    is(messages[5][0], "create", "sixth message check");
    is(messages[5][1], 6, "sixth message args");
  }

  function checkSecondBatch () {
    is(messages[6][0], "create", "seventh message check");
    is(messages[6][1], 10, "seventh message args");
    is(messages[7][0], "connect", "eighth message check");
    is(messages[7][1], 1, "eighth message args");
    is(messages[7][2], 10, "eighth message args");
  }

  return promise;
});
