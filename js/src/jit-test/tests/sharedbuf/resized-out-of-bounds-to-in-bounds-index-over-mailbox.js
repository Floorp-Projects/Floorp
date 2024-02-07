// |jit-test| --enable-arraybuffer-resizable; skip-if: !this.SharedArrayBuffer?.prototype?.grow||helperThreadCount()===0

let gsab = new SharedArrayBuffer(3, {maxByteLength: 4});

setSharedObject(gsab);

function worker(gsab) {
  let ta = new Int8Array(gsab);

  // Wait until `valueOf` is called.
  while (Atomics.load(ta, 0) === 0);

  // Now grow the buffer.
  gsab.grow(4);

  // Notify the buffer has been resized.
  Atomics.store(ta, 1, 1);
}

evalInWorker(`(${worker})(getSharedObject());`);

let ta = new Int8Array(gsab);

let value = {
  valueOf() {
    // Notify we're in `valueOf()`.
    Atomics.store(ta, 0, 1);

    // Wait until buffer has been resized.
    while (Atomics.load(ta, 1) === 0);

    // Continue execution.
    return 0;
  }
};

// Write into currently out-of-bounds, but later in-bounds index.
ta[3] = value;
