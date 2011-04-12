// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function assertThrows(f, ctor, msg) {
    var fullmsg;
    try {
        f();
    } catch (exc) {
        if (exc instanceof ctor)
            return;
        fullmsg = "Assertion failed: expected exception " + ctor.name + ", got " + exc;
    }
    if (fullmsg === undefined)
        fullmsg = "Assertion failed: expected exception " + ctor.name +", no exception thrown";
    if (msg !== undefined)
        fullmsg += " - " + msg;
    throw new Error(fullmsg);
}

assertEq(WeakMap.prototype.hasOwnProperty("has"), true);
assertEq(WeakMap.prototype.hasOwnProperty("get"), true);
assertEq(WeakMap.prototype.hasOwnProperty("set"), true);
assertEq(WeakMap.prototype.hasOwnProperty("delete"), true);

var key = {};
assertEq(WeakMap.prototype.has(key), false);
assertEq(WeakMap.prototype.get(key, undefined), undefined);
assertEq(WeakMap.prototype.get(key, 0), 0);
assertEq(WeakMap.prototype.set(key, 4), undefined);
assertEq(WeakMap.prototype.delete(key), true);

// Methods should throw when called passing undefined as the this value.
var f;
f = WeakMap.prototype.has;    assertThrows(function () { f({}); }, TypeError);
f = WeakMap.prototype.get;    assertThrows(function () { f({}); }, TypeError);
f = WeakMap.prototype.set;    assertThrows(function () { f({}, 0); }, TypeError);
f = WeakMap.prototype.delete; assertThrows(function () { f({}); }, TypeError);

// Methods should throw when applied to objects that aren't WeakMaps.
var obj = Object.create(new WeakMap);
obj.has = WeakMap.prototype.has;
obj.get = WeakMap.prototype.get;
obj.set = WeakMap.prototype.set;
obj.delete = WeakMap.prototype.delete;
assertThrows(function () { obj.has({}); }, TypeError);
assertThrows(function () { obj.get({}); }, TypeError);
assertThrows(function () { obj.set({}, 0); }, TypeError);
assertThrows(function () { obj.delete({}); }, TypeError);

// Test operations on an empty map.
var map = WeakMap();
assertEq(Object.getOwnPropertyNames(map).length, 0);
assertEq(map.has(key), false);
assertEq(map.get(key), undefined);
assertEq(map.get(key, Math.sin), Math.sin);
assertEq(map.has(key), false);
assertEq(map.delete(key), false);

// Methods should throw when passed the wrong type or not enough arguments.
assertThrows(function () { map.has(); }, TypeError);
assertThrows(function () { map.has(undefined); }, TypeError);
assertThrows(function () { map.has(null); }, TypeError);
assertThrows(function () { map.has(true); }, TypeError);
assertThrows(function () { map.has(15); }, TypeError);
assertThrows(function () { map.has("x"); }, TypeError);

assertThrows(function () { map.get(); }, TypeError);
assertThrows(function () { map.get(undefined); }, TypeError);
assertThrows(function () { map.get(null); }, TypeError);
assertThrows(function () { map.get(true); }, TypeError);
assertThrows(function () { map.get(15); }, TypeError);
assertThrows(function () { map.get("x"); }, TypeError);

assertThrows(function () { map.set(); }, TypeError);
assertThrows(function () { map.set(undefined, 0); }, TypeError);
assertThrows(function () { map.set(null, 0); }, TypeError);
assertThrows(function () { map.set(true, 0); }, TypeError);
assertThrows(function () { map.set(15, 0); }, TypeError);
assertThrows(function () { map.set("x", 0); }, TypeError);

assertThrows(function () { map.delete(); }, TypeError);
assertThrows(function () { map.delete(undefined); }, TypeError);
assertThrows(function () { map.delete(null); }, TypeError);
assertThrows(function () { map.delete(true); }, TypeError);
assertThrows(function () { map.delete(15); }, TypeError);
assertThrows(function () { map.delete("x"); }, TypeError);

// Test after setting an entry.
map.set(key, 42);
assertEq(Object.getOwnPropertyNames(map).length, 0);
assertEq(map.has(key), true);
assertEq(map.get(key), 42);
assertEq(map.get({}), undefined);
assertEq(map.get({}, "foo"), "foo");

// Test setting with just one argument.
map.set(key);
assertEq(map.get(key), undefined);
assertEq(map.has(key), true);

// Test that GC does not collect entries that are still reachable.
map.set(key, 42);
gc(); gc(); gc();
assertEq(map.get(key), 42);
assertEq(map.delete(key), true);
assertEq(typeof map.get(key), "undefined");
assertEq(map.has(key), false);
assertEq(map.delete(key), false);

// Make sure the iterative GC works.
var i;
map.set(key, 42);
for (i = 0; i < 99; i++) {
    var k = {};
    assertEq(map.set(k, key), undefined);
    key = k;
}
gc(); gc(); gc();
for (i = 0; i < 99; i++) {
    assertEq(map.has(key), true);
    key = map.get(key);
}
assertEq(map.get(key), 42);

// Another iterative GC test.
for (i = 0; i < 99; i++) {
    var m = new WeakMap();
    m.set(m, map);
    map = m;
}
gc(); gc(); gc();
for (i = 0; i < 99; i++) {
    map = map.get(map);
}
assertEq(map.get(key), 42);

reportCompare(0, 0, "ok");
