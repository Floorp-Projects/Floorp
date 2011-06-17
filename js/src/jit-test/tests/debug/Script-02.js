// |jit-test| debug
// Debug.Script throws when applied as a constructor.

load(libdir + 'asserts.js');

assertThrowsInstanceOf(function() { Debug.Script(); }, TypeError);
assertThrowsInstanceOf(function() { new Debug.Script(); }, TypeError);
