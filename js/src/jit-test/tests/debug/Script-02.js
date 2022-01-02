// Debugger.Script throws when applied as a constructor.

load(libdir + 'asserts.js');

assertThrowsInstanceOf(function() { Debugger.Script(); }, TypeError);
assertThrowsInstanceOf(function() { new Debugger.Script(); }, TypeError);
