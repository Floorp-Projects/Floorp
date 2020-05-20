// |reftest| skip-if(!this.Atomics||!this.SharedArrayBuffer) fails-if(!xulRuntime.shell)

const otherGlobal = newGlobal();

const intArrayConstructors = [
  otherGlobal.Int32Array,
  otherGlobal.Int16Array,
  otherGlobal.Int8Array,
  otherGlobal.Uint32Array,
  otherGlobal.Uint16Array,
  otherGlobal.Uint8Array,
];

// Atomics.load
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 1;

  assertEq(Atomics.load(ta, 0), 1);
}

// Atomics.store
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));

  Atomics.store(ta, 0, 1);

  assertEq(ta[0], 1);
}

// Atomics.compareExchange
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 1;

  let val = Atomics.compareExchange(ta, 0, 1, 2);

  assertEq(val, 1);
  assertEq(ta[0], 2);
}

// Atomics.exchange
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 1;

  let val = Atomics.exchange(ta, 0, 2);

  assertEq(val, 1);
  assertEq(ta[0], 2);
}

// Atomics.add
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 1;

  let val = Atomics.add(ta, 0, 2);

  assertEq(val, 1);
  assertEq(ta[0], 3);
}

// Atomics.sub
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 3;

  let val = Atomics.sub(ta, 0, 2);

  assertEq(val, 3);
  assertEq(ta[0], 1);
}

// Atomics.and
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 3;

  let val = Atomics.and(ta, 0, 1);

  assertEq(val, 3);
  assertEq(ta[0], 1);
}

// Atomics.or
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 2;

  let val = Atomics.or(ta, 0, 1);

  assertEq(val, 2);
  assertEq(ta[0], 3);
}

// Atomics.xor
for (let TA of intArrayConstructors) {
  let ta = new TA(new otherGlobal.SharedArrayBuffer(4));
  ta[0] = 3;

  let val = Atomics.xor(ta, 0, 1);

  assertEq(val, 3);
  assertEq(ta[0], 2);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
