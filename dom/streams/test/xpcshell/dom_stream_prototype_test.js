"use strict";

var log = [];
const stream = new ReadableStream({
  start(controller) {
    log.push("started");
  },
  pull(controller) {
    log.push("pulled");
    controller.enqueue("hi from pull");
  },
  cancel() {
    log.push("cancelled");
  },
});

print(log); // Currently prints "started"!

add_task(async function helper() {
  var reader = stream.getReader();
  var readPromise = reader.read();
  readPromise.then(x => print(`Printing promise result ${x}, log ${log}`));
  print(log);

  var x = await readPromise;
  print(`Promise result ${x} ${x.value}`);
  Assert.equal(x.value, "hi from pull");
  Assert.ok(true);
});
