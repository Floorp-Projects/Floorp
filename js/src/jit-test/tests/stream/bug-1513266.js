// |jit-test| --no-ion; --no-baseline; skip-if: !('oomTest' in this)

ignoreUnhandledRejections();

function test() {
  let controller;
  let stream = new ReadableStream({
    start(c) {}
  });
  stream.getReader();
  drainJobQueue();
}

// Force lazy parsing to happen before oomTest starts (see `help(oomTest)`).
test();
oomTest(test, {verbose: true, keepFailing: false});
