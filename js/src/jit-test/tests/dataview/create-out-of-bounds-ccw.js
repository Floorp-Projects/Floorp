// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

// RangeError is from the correct global when resizable ArrayBuffer gets out-of-bounds.

let g = newGlobal();

let rab = new g.ArrayBuffer(10, {maxByteLength: 10});

let newTarget = Object.defineProperty(function(){}.bind(), "prototype", {
  get() {
    rab.resize(0);
    return DataView.prototype;
  }
});

let err;
try {
  Reflect.construct(DataView, [rab, 10], newTarget);
} catch (e) {
  err = e;
}

assertEq(err instanceof RangeError, true);
