// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize||!this.SharedArrayBuffer||helperThreadCount()===0

function setup() {
  // Shared memory locations:
  //
  // 0: Lock
  // 1: Sleep
  // 2: Data
  // 3: Unused

  function worker(gsab) {
    var ta = new Int32Array(gsab);

    // Notify the main thread that the worker is ready.
    Atomics.store(ta, 0, 1);

    // Sleep to give the main thread time to execute and tier-up the loop.
    Atomics.wait(ta, 1, 0, 500);

    // Modify the memory read in the loop.
    Atomics.store(ta, 2, 1);

    // Sleep again to give the main thread time to execute the loop.
    Atomics.wait(ta, 1, 0, 100);

    // Grow the buffer. This modifies the loop condition.
    gsab.grow(16);
  }

  var gsab = new SharedArrayBuffer(12, {maxByteLength: 16});

  // Pass |gsab| to the mailbox.
  setSharedObject(gsab);

  // Start the worker.
  evalInWorker(`
    (${worker})(getSharedObject());
  `);

  // Wait until worker is ready.
  var ta = new Int32Array(gsab);
  while (Atomics.load(ta, 0) === 0);

  return gsab;
}

function testTypedArrayLength() {
  var gsab = setup();
  var ta = new Int32Array(gsab);
  var r = 0;

  // |ta.length| is a seq-cst load, so it must prevent reordering any other
  // loads, including unordered loads like |ta[2]|.
  while (ta.length <= 3) {
    // |ta[2]| is an unordered load, so it's hoistable by default.
    r += ta[2];
  }

  // The memory location is first modified and then the buffer is grown, so we
  // must observe reads of the modified memory location before exiting the loop.
  assertEq(
    r > 0,
    true,
    "ta.length acts as a memory barrier, so ta[2] can't be hoisted"
  );
}
testTypedArrayLength();
