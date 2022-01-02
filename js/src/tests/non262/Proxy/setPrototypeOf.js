/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "setPrototypeOf.js";
var BUGNUMBER = 888969;
var summary = "Scripted proxies' [[SetPrototypeOf]] behavior";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

const log = [];

function observe(obj)
{
  var observingHandler = new Proxy({}, {
    get(target, p, receiver) {
      log.push(p);
      return Reflect.get(target, p, receiver);
    }
  });

  return new Proxy(obj, observingHandler);
}

var p, h;

// 1. Assert: Either Type(V) is Object or Type(V) is Null.
// 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
// 3. If handler is null, throw a TypeError exception.
// 4. Assert: Type(handler) is Object.
// 5. Let target be the value of the [[ProxyTarget]] internal slot of O.

var rev = Proxy.revocable({}, {});
p = rev.proxy;

var originalProto = Reflect.getPrototypeOf(p);
assertEq(originalProto, Object.prototype);

rev.revoke();
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, originalProto),
                       TypeError);
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

// 6. Let trap be ? GetMethod(handler, "setPrototypeOf").

// handler has uncallable (and not null/undefined) property
p = new Proxy({}, { setPrototypeOf: 9 });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: -3.7 });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: NaN });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: Infinity });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: true });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: /x/ });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: Symbol(42) });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, { setPrototypeOf: class X {} });
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

p = new Proxy({}, observe({}));

assertEq(Reflect.setPrototypeOf(p, Object.prototype), true);
assertEq(log.length, 1);
assertEq(log[0], "get");

h = observe({ setPrototypeOf() { throw 3.14; } });
p = new Proxy(Object.create(Object.prototype), h);

// "setting" without change
log.length = 0;
assertThrowsValue(() => Reflect.setPrototypeOf(p, Object.prototype),
                  3.14);
assertEq(log.length, 1);
assertEq(log[0], "get");

// "setting" with change
log.length = 0;
assertThrowsValue(() => Reflect.setPrototypeOf(p, /foo/),
                  3.14);
assertEq(log.length, 1);
assertEq(log[0], "get");

// 7. If trap is undefined, then
//    a. Return ? target.[[SetPrototypeOf]](V).

var settingProtoThrows =
  new Proxy({}, { setPrototypeOf() { throw "agnizes"; } });

p = new Proxy(settingProtoThrows, { setPrototypeOf: undefined });
assertThrowsValue(() => Reflect.setPrototypeOf(p, null),
                  "agnizes");

p = new Proxy(settingProtoThrows, { setPrototypeOf: null });
assertThrowsValue(() => Reflect.setPrototypeOf(p, null),
                  "agnizes");

var anotherProto =
  new Proxy({},
            { setPrototypeOf(t, p) {
                log.push("reached");
                return Reflect.setPrototypeOf(t, p);
              } });

p = new Proxy(anotherProto, { setPrototypeOf: undefined });

log.length = 0;
assertEq(Reflect.setPrototypeOf(p, null), true);
assertEq(log.length, 1);
assertEq(log[0], "reached");

p = new Proxy(anotherProto, { setPrototypeOf: null });

log.length = 0;
assertEq(Reflect.setPrototypeOf(p, null), true);
assertEq(log.length, 1);
assertEq(log[0], "reached");

// 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler, « target, V »)).

// The trap callable might throw.
p = new Proxy({}, { setPrototypeOf() { throw "ohai"; } });

assertThrowsValue(() => Reflect.setPrototypeOf(p, /x/),
                  "ohai");

var throwingTrap =
  new Proxy(function() { throw "not called"; },
            { apply() { throw 37; } });

p = new Proxy({}, { setPrototypeOf: throwingTrap });

assertThrowsValue(() => Reflect.setPrototypeOf(p, Object.prototype),
                  37);

// The trap callable must *only* be called.
p = new Proxy({},
              {
                setPrototypeOf: observe(function() { throw "boo-urns"; })
              });

log.length = 0;
assertThrowsValue(() => Reflect.setPrototypeOf(p, Object.prototype),
                  "boo-urns");

assertEq(log.length, 1);
assertEq(log[0], "apply");

// 9. If booleanTrapResult is false, return false.

p = new Proxy({}, { setPrototypeOf() { return false; } });
assertEq(Reflect.setPrototypeOf(p, Object.prototype), false);

p = new Proxy({}, { setPrototypeOf() { return +0; } });
assertEq(Reflect.setPrototypeOf(p, Object.prototype), false);

p = new Proxy({}, { setPrototypeOf() { return -0; } });
assertEq(Reflect.setPrototypeOf(p, Object.prototype), false);

p = new Proxy({}, { setPrototypeOf() { return NaN; } });
assertEq(Reflect.setPrototypeOf(p, Object.prototype), false);

p = new Proxy({}, { setPrototypeOf() { return ""; } });
assertEq(Reflect.setPrototypeOf(p, Object.prototype), false);

p = new Proxy({}, { setPrototypeOf() { return undefined; } });
assertEq(Reflect.setPrototypeOf(p, Object.prototype), false);

// 10. Let extensibleTarget be ? IsExtensible(target).

var targetThrowIsExtensible =
  new Proxy({}, { isExtensible() { throw "psych!"; } });

p = new Proxy(targetThrowIsExtensible, { setPrototypeOf() { return true; } });
assertThrowsValue(() => Reflect.setPrototypeOf(p, Object.prototype),
                  "psych!");

// 11. If extensibleTarget is true, return true.

var targ = {};

p = new Proxy(targ, { setPrototypeOf() { return true; } });
assertEq(Reflect.setPrototypeOf(p, /x/), true);
assertEq(Reflect.getPrototypeOf(targ), Object.prototype);
assertEq(Reflect.getPrototypeOf(p), Object.prototype);

p = new Proxy(targ, { setPrototypeOf() { return /x/; } });
assertEq(Reflect.setPrototypeOf(p, /x/), true);
assertEq(Reflect.getPrototypeOf(targ), Object.prototype);
assertEq(Reflect.getPrototypeOf(p), Object.prototype);

p = new Proxy(targ, { setPrototypeOf() { return Infinity; } });
assertEq(Reflect.setPrototypeOf(p, /x/), true);
assertEq(Reflect.getPrototypeOf(targ), Object.prototype);
assertEq(Reflect.getPrototypeOf(p), Object.prototype);

p = new Proxy(targ, { setPrototypeOf() { return Symbol(true); } });
assertEq(Reflect.setPrototypeOf(p, /x/), true);
assertEq(Reflect.getPrototypeOf(targ), Object.prototype);
assertEq(Reflect.getPrototypeOf(p), Object.prototype);

// 12. Let targetProto be ? target.[[GetPrototypeOf]]().

var targetNotExtensibleGetProtoThrows =
  new Proxy(Object.preventExtensions({}),
            { getPrototypeOf() { throw NaN; } });

p = new Proxy(targetNotExtensibleGetProtoThrows,
              { setPrototypeOf() { log.push("goober"); return true; } });

log.length = 0;
assertThrowsValue(() => Reflect.setPrototypeOf(p, /abcd/),
                  NaN);

// 13. If SameValue(V, targetProto) is false, throw a TypeError exception.

var newProto;

p = new Proxy(Object.preventExtensions(Object.create(Math)),
              { setPrototypeOf(t, p) { return true; } });

assertThrowsInstanceOf(() => Reflect.setPrototypeOf(p, null),
                       TypeError);

// 14. Return true.

assertEq(Reflect.setPrototypeOf(p, Math), true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
