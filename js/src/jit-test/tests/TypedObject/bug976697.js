// Test that instantiating a typed array on top of a neutered buffer
// doesn't trip any asserts.
//
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

x = ArrayBuffer();
neuter(x, "same-data");
Uint32Array(x);
gc();

x = ArrayBuffer();
neuter(x, "change-data");
Uint32Array(x);
gc();
