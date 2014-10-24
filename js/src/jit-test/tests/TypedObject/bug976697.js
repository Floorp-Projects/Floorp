// Test that instantiating a typed array on top of a neutered buffer
// doesn't trip any asserts.
//
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

x = new ArrayBuffer();
neuter(x, "same-data");
new Uint32Array(x);
gc();

x = new ArrayBuffer();
neuter(x, "change-data");
new Uint32Array(x);
gc();
