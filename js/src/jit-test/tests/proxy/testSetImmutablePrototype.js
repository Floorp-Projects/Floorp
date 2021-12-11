load(libdir + "asserts.js");

let p = {};
let x = new Proxy({__proto__: p}, {});
assertEq(Reflect.getPrototypeOf(x), p);
setImmutablePrototype(x);
assertEq(Reflect.getPrototypeOf(x), p);
assertEq(Reflect.setPrototypeOf(x, Date.prototype), false);
assertEq(Reflect.setPrototypeOf(x, p), true);
assertThrowsInstanceOf(() => Object.setPrototypeOf(x, Date.prototype), TypeError);
assertEq(Reflect.getPrototypeOf(x), p);
