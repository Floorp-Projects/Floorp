/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'trap-null.js';
var BUGNUMBER = 1257102;
var summary = "null as a trap value on a handler should operate on the target";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// This might seem like overkill, but this proxying trick caught typos of
// several trap names before this test landed.  \o/  /o\
var allTraps = {
  getPrototypeOf: null,
  setPrototypeOf: null,
  isExtensible: null,
  preventExtensions: null,
  getOwnPropertyDescriptor: null,
  defineProperty: null,
  has: null,
  get: null,
  set: null,
  deleteProperty: null,
  ownKeys: null,
  apply: null,
  construct: null,
};

var complainingHandler = new Proxy(allTraps, {
  get(target, p, receiver) {
    var v = Reflect.get(target, p, receiver);
    if (v !== null)
      throw new TypeError("failed to set one of the traps to null?  " + p);

    return v;
  }
});

var proxyTarget = function(x) {
  "use strict";
  var str = this + x;
  str += new.target ? "constructing" : "calling";
  return new.target ? new String(str) : str;
};
proxyTarget.prototype.toString = () => "###";
proxyTarget.prototype.valueOf = () => "@@@";

var proxy = new Proxy(proxyTarget, complainingHandler);

assertEq(Reflect.getPrototypeOf(proxy), Function.prototype);

assertEq(Object.getPrototypeOf(proxyTarget), Function.prototype);
assertEq(Reflect.setPrototypeOf(proxy, null), true);
assertEq(Object.getPrototypeOf(proxyTarget), null);

assertEq(Reflect.isExtensible(proxy), true);

assertEq(Reflect.isExtensible(proxyTarget), true);
assertEq(Reflect.preventExtensions(proxy), true);
assertEq(Reflect.isExtensible(proxy), false);
assertEq(Reflect.isExtensible(proxyTarget), false);

var desc = Reflect.getOwnPropertyDescriptor(proxy, "length");
assertEq(desc.value, 1);

assertEq(desc.configurable, true);
assertEq(Reflect.defineProperty(proxy, "length", { value: 3, configurable: false }), true);
desc = Reflect.getOwnPropertyDescriptor(proxy, "length");
assertEq(desc.configurable, false);

assertEq(Reflect.has(proxy, "length"), true);

assertEq(Reflect.get(proxy, "length"), 3);

assertEq(Reflect.set(proxy, "length", 3), false);

assertEq(Reflect.deleteProperty(proxy, "length"), false);

var keys = Reflect.ownKeys(proxy);
assertEq(keys.length, 3);
keys.sort();
assertEq(keys[0], "length");
assertEq(keys[1], "name");
assertEq(keys[2], "prototype");

assertEq(Reflect.apply(proxy, "hi!", [" "]), "hi! calling");

var res = Reflect.construct(proxy, [" - "]);
assertEq(typeof res, "object");
assertEq(res instanceof String, true);
assertEq(res.valueOf(), "@@@ - constructing");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
