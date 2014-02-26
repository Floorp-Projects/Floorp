// Test that instantiating a typed array on top of a neutered buffer
// doesn't trip any asserts. Public domain.

if (!this.hasOwnProperty("TypedObject"))
  quit();

x = ArrayBuffer();
neuter(x);
Uint32Array(x);
gc();
