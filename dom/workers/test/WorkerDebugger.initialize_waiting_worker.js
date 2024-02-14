"use strict";

// SharedArrayBuffer isn't available...
const { buffer } = new WebAssembly.Memory({
  shared: true,
  initial: 1,
  maximum: 1,
});
const int32 = new Int32Array(buffer);

postMessage("worker initialized");

// This should block the worker thread synchronously
console.log("Calling Atomics.wait");
Atomics.wait(int32, 0, 0);
console.log("Atomics.wait returned");
