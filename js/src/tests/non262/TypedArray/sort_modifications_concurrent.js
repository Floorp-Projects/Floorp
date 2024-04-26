// |reftest| shell-option(--enable-float16array) skip-if(!xulRuntime.shell)

if (helperThreadCount() === 0) {
  if (typeof reportCompare === "function")
    reportCompare(true, true);
  quit();
}

const TAConstructors = [
  Int8Array,
  Uint8Array,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array,
  BigInt64Array,
  BigUint64Array,
].concat(this.Float16Array ?? []);

// Use different size classes to catch any implementation-specific
// optimisations.
const sizes = [
  4, 8, 64, 128, 1024
];

function ToNumeric(TA) {
  if (TA === BigInt64Array || TA === BigUint64Array) {
    return BigInt;
  }
  return Number;
}

function ToAtomicTA(TA) {
  switch (TA) {
  case Int8Array:
  case Int16Array:
  case Int32Array:
  case Uint8Array:
  case Uint16Array:
  case Uint32Array:
  case BigInt64Array:
  case BigUint64Array:
    return TA;
  case Uint8ClampedArray:
    return Uint8Array;
  case globalThis.Float16Array:
    return Uint16Array;
  case Float32Array:
    return Uint32Array;
  case Float64Array:
    return BigUint64Array;
  }
  throw new Error("Invalid typed array kind");
}

function ascending(a, b) {
  return a < b ? -1 : a > b ? 1 : 0;
}

function descending(a, b) {
  return -ascending(a, b);
}

for (let TA of TAConstructors) {
  let toNumeric = ToNumeric(TA);
  for (let size of sizes) {
    let sorted = new TA(size);

    // Fill with |1..size| and then sort to account for wrap-arounds.
    for (let i = 0; i < size; ++i) {
      sorted[i] = toNumeric(i + 1);
    }
    sorted.sort();

    let extra = Math.max(TA.BYTES_PER_ELEMENT, Int32Array.BYTES_PER_ELEMENT);
    let buffer = new SharedArrayBuffer(size * TA.BYTES_PER_ELEMENT + extra);
    let controller = new Int32Array(buffer, 0, 1);

    // Create a copy in descending order.
    let ta = new TA(buffer, extra, size);
    ta.set(sorted)
    ta.sort(descending);

    // Worker code expects that the last element changes when sorted.
    assertEq(ta[size - 1] === sorted[size - 1], false);

    setSharedObject(buffer);

    evalInWorker(`
      const ToNumeric = ${ToNumeric};
      const ToAtomicTA = ${ToAtomicTA};
      const TA = ${TA.name};
      const AtomicTA = ToAtomicTA(TA);

      let size = ${size};
      let extra = ${extra};

      let toNumeric = ToNumeric(AtomicTA);
      let buffer = getSharedObject();
      let controller = new Int32Array(buffer, 0, 1);
      let ta = new AtomicTA(buffer, extra, size);

      let value = Atomics.load(ta, size - 1);

      // Coordinate with main thread.
      while (Atomics.notify(controller, 0, 1) < 1) ;

      // Wait until modification of the last element.
      //
      // Sorting writes in ascending indexed ordered, so when the last element
      // was modified, we know that the sort operation has finished.
      while (Atomics.load(ta, size - 1) === value) ;

      // Set all elements to zero.
      ta.fill(toNumeric(0));

      // Sleep for 50 ms.
      const amount = 0.05;

      // Coordinate with main thread.
      while (Atomics.notify(controller, 0, 1) < 1) {
        sleep(amount);
      }
    `);

    // Wait until worker is set-up.
    assertEq(Atomics.wait(controller, 0, 0), "ok");

    // Sort the array in ascending order.
    ta.sort();

    // Wait until worker has finished.
    assertEq(Atomics.wait(controller, 0, 0), "ok");

    // All elements have been set to zero.
    let zero = toNumeric(0);
    for (let i = 0; i < size; ++i) {
      assertEq(ta[i], zero, `${TA.name} at index ${i} for size ${size}`);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
