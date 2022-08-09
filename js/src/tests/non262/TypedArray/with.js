// |reftest| shell-option(--enable-change-array-by-copy) skip-if(!Int32Array.prototype.with)

class Err {}

const indices = [
  -Infinity, -10, -0.5, -0, 0, 0.5, 10, Infinity, NaN
];

let value = {
  valueOf() {
    throw new Err;
  }
};

let ta = new Int32Array(5);
for (let index of indices) {
  assertThrowsInstanceOf(() => ta.with(index, value), Err);
}

for (let index of indices) {
  let ta = new Int32Array(5);

  let value = {
    valueOf() {
      detachArrayBuffer(ta.buffer);
      return 0;
    }
  };

  assertThrowsInstanceOf(() => ta.with(index, value), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
