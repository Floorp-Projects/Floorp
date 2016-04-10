// |reftest| skip-if(!xulRuntime.shell) -- needs enableMatchFlagArgument()

var BUGNUMBER = 1263139;
var summary = "String.prototype.match with non-string non-standard flags argument.";

print(BUGNUMBER + ": " + summary);

enableMatchFlagArgument();

var called;
var flags = {
  toString() {
    called = true;
    return "";
  }
};

called = false;
"a".match("a", flags);
assertEq(called, true);

called = false;
"a".search("a", flags);
assertEq(called, true);

called = false;
"a".replace("a", "b", flags);
assertEq(called, true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
