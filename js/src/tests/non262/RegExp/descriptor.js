var BUGNUMBER = 1120169;
var summary = "Implement RegExp.prototype.{global, ignoreCase, multiline, sticky, unicode} - property descriptor";

print(BUGNUMBER + ": " + summary);

var getters = [
  "flags",
  "global",
  "ignoreCase",
  "multiline",
  "source",
  "sticky",
  "unicode",
];

for (var name of getters) {
  var desc = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
  assertEq(desc.configurable, true);
  assertEq(desc.enumerable, false);
  assertEq("writable" in desc, false);
  assertEq("get" in desc, true);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
