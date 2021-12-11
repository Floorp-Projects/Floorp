// Test that SavedFrame instances are frozen and can't be messed with.

// Strict mode so that mutating frozen objects doesn't silently fail.
"use strict";

const s = saveStack();

load(libdir + 'asserts.js');

assertThrowsInstanceOf(() => s.source = "fake.url",
                       TypeError);

assertThrowsInstanceOf(() => {
  Object.defineProperty(s.__proto__, "line", {
    get: () => 0
  })
}, TypeError);
