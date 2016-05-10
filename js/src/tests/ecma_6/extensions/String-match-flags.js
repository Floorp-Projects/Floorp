var BUGNUMBER = 1263139;
var summary = "String.prototype.match with non-string non-standard flags argument.";

print(BUGNUMBER + ": " + summary);

var called;
var flags = {
  toString() {
    called = true;
    return "";
  }
};

called = false;
"a".match("a", flags);
assertEq(called, false);

called = false;
"a".search("a", flags);
assertEq(called, false);

called = false;
"a".replace("a", "b", flags);
assertEq(called, false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
