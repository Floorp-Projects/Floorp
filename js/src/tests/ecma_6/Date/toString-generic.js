var BUGNUMBER = 861219;
var summary = 'Date.prototype.toString is a generic function';

// Revised in ECMA 2018, Date.prototype.toString is no longer generic (bug 1381433).

print(BUGNUMBER + ": " + summary);

for (var thisValue of [{}, [], /foo/, Date.prototype, new Proxy(new Date(), {})])
  assertThrowsInstanceOf(() => Date.prototype.toString.call(thisValue), TypeError);

for (var prim of [null, undefined, 0, 1.2, true, false, "foo", Symbol.iterator])
  assertThrowsInstanceOf(() => Date.prototype.toString.call(prim), TypeError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
