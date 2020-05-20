load(libdir + "asserts.js");

const otherGlobal = newGlobal({newCompartment: true});

function nukeValue(wrapper, value) {
  return {
    valueOf() {
      nukeCCW(wrapper);
      return value;
    }
  };
};

function assertIsDeadWrapper(value) {
  assertTypeErrorMessage(() => value.foo, "can't access dead object");
}

// Atomics.load
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.load(ta, nukeValue(ta, 0));

  assertEq(val, 1);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.load(ta, nukeValue(sab, 0));

  assertEq(val, 1);
  assertEq(ta[0], 1);
  assertIsDeadWrapper(sab);
}

// Atomics.store
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);

  Atomics.store(ta, 0, nukeValue(ta, 1));

  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);

  Atomics.store(ta, 0, nukeValue(sab, 1));

  assertEq(ta[0], 1);
  assertIsDeadWrapper(sab);
}

// Atomics.compareExchange
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.compareExchange(ta, 0, 1, nukeValue(ta, 2));

  assertEq(val, 1);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.compareExchange(ta, 0, 1, nukeValue(sab, 2));

  assertEq(val, 1);
  assertEq(ta[0], 2);
  assertIsDeadWrapper(sab);
}

// Atomics.exchange
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.exchange(ta, 0, nukeValue(ta, 2));

  assertEq(val, 1);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.exchange(ta, 0, nukeValue(sab, 2));

  assertEq(val, 1);
  assertEq(ta[0], 2);
  assertIsDeadWrapper(sab);
}

// Atomics.add
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.add(ta, 0, nukeValue(ta, 2));

  assertEq(val, 1);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 1;

  let val = Atomics.add(ta, 0, nukeValue(sab, 2));

  assertEq(val, 1);
  assertEq(ta[0], 3);
  assertIsDeadWrapper(sab);
}

// Atomics.sub
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 3;

  let val = Atomics.sub(ta, 0, nukeValue(ta, 2));

  assertEq(val, 3);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 3;

  let val = Atomics.sub(ta, 0, nukeValue(sab, 2));

  assertEq(val, 3);
  assertEq(ta[0], 1);
  assertIsDeadWrapper(sab);
}

// Atomics.and
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 3;

  let val = Atomics.and(ta, 0, nukeValue(ta, 1));

  assertEq(val, 3);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 3;

  let val = Atomics.and(ta, 0, nukeValue(sab, 1));

  assertEq(val, 3);
  assertEq(ta[0], 1);
  assertIsDeadWrapper(sab);
}

// Atomics.or
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 2;

  let val = Atomics.or(ta, 0, nukeValue(ta, 1));

  assertEq(val, 2);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 2;

  let val = Atomics.or(ta, 0, nukeValue(sab, 1));

  assertEq(val, 2);
  assertEq(ta[0], 3);
  assertIsDeadWrapper(sab);
}

// Atomics.xor
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 3;

  let val = Atomics.xor(ta, 0, nukeValue(ta, 1));

  assertEq(val, 3);
  assertIsDeadWrapper(ta);
}
{
  let sab = new otherGlobal.SharedArrayBuffer(4);
  let ta = new otherGlobal.Int32Array(sab);
  ta[0] = 3;

  let val = Atomics.xor(ta, 0, nukeValue(sab, 1));

  assertEq(val, 3);
  assertEq(ta[0], 2);
  assertIsDeadWrapper(sab);
}
