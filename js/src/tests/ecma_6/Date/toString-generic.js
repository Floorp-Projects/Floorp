var BUGNUMBER = 861219;
var summary = 'Date.prototype.toString is a generic function';

print(BUGNUMBER + ": " + summary);

for (var thisValue of [{}, [], /foo/, Date.prototype, new Proxy(new Date(), {})])
  assertEq(Date.prototype.toString.call(thisValue), "Invalid Date");

for (var prim of [null, undefined, 0, 1.2, true, false, "foo", Symbol.iterator])
  assertThrowsInstanceOf(() => Date.prototype.toString.call(prim), TypeError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
