if (!this.SharedArrayBuffer)
    quit(0);

load(libdir + "asserts.js");

var sab = new SharedArrayBuffer(1 * Int32Array.BYTES_PER_ELEMENT);

// Make a copy, sharing the same memory
var sab2 = (setSharedObject(sab), getSharedObject());

// Assert it's not the same object
assertEq(sab === sab2, false);

// Assert they're sharing memory
new Int32Array(sab)[0] = 0x12345678;
assertEq(new Int32Array(sab2)[0], 0x12345678)

sab.constructor = {
  [Symbol.species]: function(length) {
    return sab2;
  }
};

// This should throw because the buffer being sliced shares memory with the new
// buffer it constructs.
assertThrowsInstanceOf(() => sab.slice(), TypeError);
