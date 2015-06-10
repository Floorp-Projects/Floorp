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
  //"unicode",
];

for (var name of getters) {
  var desc = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
  assertEq(desc.configurable, true);
  assertEq(desc.enumerable, false);
  assertEq("writable" in desc, false);
  assertEq("get" in desc, true);
}

// When the /u flag is supported, remove this comment and the next line, and
// uncomment "unicode" in |props| above.
assertThrowsInstanceOf(() => RegExp("", "mygui").flags, SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
