// |reftest| skip-if(!this.Atomics)

const intArrayConstructors = [
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array,
];

function badValue(ta) {
  return {
    valueOf() {
      detachArrayBuffer(ta.buffer);
      return 0;
    }
  };
}

// Atomics.load
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.load(ta, badValue(ta)), TypeError);
}

// Atomics.store
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.store(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.store(ta, 0, badValue(ta)), TypeError);
}

// Atomics.compareExchange
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.compareExchange(ta, badValue(ta), 0, 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.compareExchange(ta, 0, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.compareExchange(ta, 0, 0, badValue(ta)), TypeError);
}

// Atomics.exchange
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.exchange(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.exchange(ta, 0, badValue(ta)), TypeError);
}

// Atomics.add
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.add(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.add(ta, 0, badValue(ta)), TypeError);
}

// Atomics.sub
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.sub(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.sub(ta, 0, badValue(ta)), TypeError);
}

// Atomics.and
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.and(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.and(ta, 0, badValue(ta)), TypeError);
}

// Atomics.or
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.or(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.or(ta, 0, badValue(ta)), TypeError);
}

// Atomics.xor
for (let TA of intArrayConstructors) {
  let ta = new TA(1);

  assertThrowsInstanceOf(() => Atomics.xor(ta, badValue(ta), 0), TypeError);
  assertThrowsInstanceOf(() => Atomics.xor(ta, 0, badValue(ta)), TypeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
