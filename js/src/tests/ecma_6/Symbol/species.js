var BUGNUMBER = 1131043;
var summary = "Implement @@species getter for builtin types";

print(BUGNUMBER + ": " + summary);

for (var C of [Map, Set]) {
  assertEq(C[Symbol.species], C);
}

for (C of [Map, Set]) {
  var desc = Object.getOwnPropertyDescriptor(C, Symbol.species);
  assertDeepEq(Object.keys(desc).sort(), ["configurable", "enumerable", "get", "set"]);
  assertEq(desc.set, undefined);
  assertEq(desc.enumerable, false);
  assertEq(desc.configurable, true);
  assertEq(desc.get.apply(null), null);
  assertEq(desc.get.apply(undefined), undefined);
  assertEq(desc.get.apply(42), 42);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
