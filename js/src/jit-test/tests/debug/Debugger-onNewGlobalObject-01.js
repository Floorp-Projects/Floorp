// Debugger.prototype.onNewGlobalObject surfaces.

load(libdir + 'asserts.js');

var dbg = new Debugger;

function f() { }
function g() { }

assertEq(Object.getOwnPropertyDescriptor(dbg, 'onNewGlobalObject'), undefined);

var d = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(dbg), 'onNewGlobalObject');
assertEq(d.enumerable, false);
assertEq(d.configurable, true);
assertEq(typeof d.get, "function");
assertEq(typeof d.set, "function");

assertEq(dbg.onNewGlobalObject, undefined);

assertThrowsInstanceOf(function () { dbg.onNewGlobalObject = ''; }, TypeError);
assertEq(dbg.onNewGlobalObject, undefined);

assertThrowsInstanceOf(function () { dbg.onNewGlobalObject = false; }, TypeError);
assertEq(dbg.onNewGlobalObject, undefined);

assertThrowsInstanceOf(function () { dbg.onNewGlobalObject = 0; }, TypeError);
assertEq(dbg.onNewGlobalObject, undefined);

assertThrowsInstanceOf(function () { dbg.onNewGlobalObject = Math.PI; }, TypeError);
assertEq(dbg.onNewGlobalObject, undefined);

assertThrowsInstanceOf(function () { dbg.onNewGlobalObject = null; }, TypeError);
assertEq(dbg.onNewGlobalObject, undefined);

assertThrowsInstanceOf(function () { dbg.onNewGlobalObject = {}; }, TypeError);
assertEq(dbg.onNewGlobalObject, undefined);

// But any function, even a useless one, is okay. How fair is that?
dbg.onNewGlobalObject = f;
assertEq(dbg.onNewGlobalObject, f);

dbg.onNewGlobalObject = undefined;
assertEq(dbg.onNewGlobalObject, undefined);

var dbg2 = new Debugger;
assertEq(dbg.onNewGlobalObject, undefined);
assertEq(dbg2.onNewGlobalObject, undefined);

dbg.onNewGlobalObject = f;
assertEq(dbg.onNewGlobalObject, f);
assertEq(dbg2.onNewGlobalObject, undefined);

dbg2.onNewGlobalObject = g;
assertEq(dbg.onNewGlobalObject, f);
assertEq(dbg2.onNewGlobalObject, g);

dbg.onNewGlobalObject = undefined;
assertEq(dbg.onNewGlobalObject, undefined);
assertEq(dbg2.onNewGlobalObject, g);

// You shouldn't be able to apply the accessor to the prototype.
assertThrowsInstanceOf(function () {
                         Debugger.prototype.onNewGlobalObject = function () { };
                       }, TypeError);
