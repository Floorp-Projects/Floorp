// Test uncatchable error when a stream's queuing strategy's size() method is called.
/* global newGlobal */

let fnFinished = false;
let g;
add_task(async function test() {
  // Make `debugger;` raise an uncatchable exception.
  g = newGlobal();
  g.parent = this;
  g.hit = false;
  g.info = info;
  g.eval(`
    var dbg = new Debugger(parent);
    dbg.onDebuggerStatement = (_frame, exc) => {hit = true; info("hit"); return null};
`);

  async function fn() {
    // Await once to postpone the uncatchable error until we're running inside
    // a reaction job. We don't want the rest of the test to be terminated.
    // (`drainJobQueue` catches uncatchable errors!)
    await 1;

    try {
      // Create a stream with a strategy whose .size() method raises an
      // uncatchable exception, and have it call that method.
      new ReadableStream(
        {
          start(controller) {
            controller.enqueue("FIRST POST"); // this calls .size()
          },
        },
        {
          size() {
            // eslint-disable-next-line no-debugger
            debugger;
          },
        }
      );
    } finally {
      fnFinished = true;
    }
  }

  fn()
    .then(() => info("Resolved"))
    .catch(() => info("Rejected"));
});

add_task(() => {
  equal(g.hit, true, "We hit G");
  equal(fnFinished, false, "We didn't hit the finally block");
});
