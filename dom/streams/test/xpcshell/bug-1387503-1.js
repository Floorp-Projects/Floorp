// Test uncatchable error when a stream source's pull() method is called.
let readerCreated = false;
let fnFinished = false;
let g;

add_task(async function test() {
  // Make `debugger;` raise an uncatchable error.
  g = newGlobal({ newCompartment: true });
  g.parent = this;
  g.hit = false;
  g.eval(
    ` new Debugger(parent).onDebuggerStatement = _frame => (hit = true, null);`
  );

  // Create a stream whose pull() method raises an uncatchable error,
  // and try reading from it.

  async function fn() {
    try {
      let stream = new ReadableStream({
        start(controller) {},
        pull(controller) {
          // eslint-disable-next-line no-debugger
          debugger;
        },
      });

      let reader = stream.getReader();
      let p = reader.read();
      readerCreated = true;
      await p;
    } finally {
      fnFinished = true;
    }
  }

  fn();
});

add_task(() => {
  equal(readerCreated, true);
  equal(g.hit, true);
  equal(fnFinished, false);
});
