// Test that instantiating a typed array on top of a neutered buffer
// doesn't trip any asserts.
//
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

if (!this.hasOwnProperty("TypedObject"))
  quit();

x = ArrayBuffer();
neuter(x);
Uint32Array(x);
gc();
