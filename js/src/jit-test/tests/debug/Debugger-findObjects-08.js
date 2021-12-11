// Passing bad query properties to Debugger.prototype.findScripts throws.

load(libdir + 'asserts.js');

var dbg = new Debugger();
var g = newGlobal();

assertThrowsInstanceOf(() => dbg.findObjects({ class: null }), TypeError);
assertThrowsInstanceOf(() => dbg.findObjects({ class: true }), TypeError);
assertThrowsInstanceOf(() => dbg.findObjects({ class: 1337 }), TypeError);
assertThrowsInstanceOf(() => dbg.findObjects({ class: /re/ }), TypeError);
assertThrowsInstanceOf(() => dbg.findObjects({ class: {}   }), TypeError);
