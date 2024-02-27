// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize||!this.SharedArrayBuffer||helperThreadCount()===0

function setup() {
  // Shared memory locations:
  //
  // 0: Lock
  // 1: Sleep
  // 2: Data
  // 3: Unused

  function worker(gsab, sab) {
    var ta = new Int32Array(gsab);
    var ta2 = new Int32Array(sab);

    // Notify the main thread that the worker is ready.
    Atomics.store(ta, 0, 1);

    // Sleep to give the main thread time to execute and tier-up the loop.
    Atomics.wait(ta, 1, 0, 500);

    // Modify the memory read in the loop.
    Atomics.store(ta2, 2, 1);

    // Sleep again to give the main thread time to execute the loop.
    Atomics.wait(ta, 1, 0, 100);

    // Grow the buffer. This modifies the loop condition.
    gsab.grow(16);
  }

  var gsab = new SharedArrayBuffer(12, {maxByteLength: 16});
  var sab = new SharedArrayBuffer(12);

  // Start the worker.
  {
    let buffers = [gsab, sab];

    // Shared memory locations:
    //
    // 0: Number of buffers
    // 1: Ready-Flag Worker
    // 2: Ready-Flag Main
    let sync = new Int32Array(new SharedArrayBuffer(3 * Int32Array.BYTES_PER_ELEMENT));
    sync[0] = buffers.length;

    setSharedObject(sync.buffer);

    evalInWorker(`
      let buffers = [];
      let sync = new Int32Array(getSharedObject());
      let n = sync[0];
      for (let i = 0; i < n; ++i) {
        // Notify we're ready to receive.
        Atomics.store(sync, 1, 1);

        // Wait until buffer is in mailbox.
        while (Atomics.compareExchange(sync, 2, 1, 0) !== 1);

        buffers.push(getSharedObject());
      }
      (${worker})(...buffers);
    `);

    for (let buffer of buffers) {
      // Wait until worker is ready.
      while (Atomics.compareExchange(sync, 1, 1, 0) !== 1);

      setSharedObject(buffer);

      // Notify buffer is in mailbox.
      Atomics.store(sync, 2, 1);
    }
  }

  // Wait until worker is ready.
  var ta = new Int32Array(gsab);
  while (Atomics.load(ta, 0) === 0);

  return {gsab, sab};
}

function testTypedArrayLength() {
  var {gsab, sab} = setup();
  var ta = new Int32Array(gsab);
  var ta2 = new Int32Array(sab);
  var r = 0;

  // |ta.length| is a seq-cst load, so it must prevent reordering any other
  // loads, including unordered loads like |ta2[2]|.
  while (ta.length <= 3) {
    // |ta2[2]| is an unordered load, so it's hoistable by default.
    r += ta2[2];
  }

  // The memory location is first modified and then the buffer is grown, so we
  // must observe reads of the modified memory location before exiting the loop.
  assertEq(
    r > 0,
    true,
    "ta.length acts as a memory barrier, so ta2[2] can't be hoisted"
  );
}
testTypedArrayLength();
