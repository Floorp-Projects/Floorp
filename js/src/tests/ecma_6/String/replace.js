var BUGNUMBER = 887016;
var summary = "Call RegExp.prototype[@@replace] from String.prototype.replace.";

print(BUGNUMBER + ": " + summary);

var called = 0;
var myRegExp = {
  [Symbol.replace](S, R) {
    assertEq(S, "abcAbcABC");
    assertEq(R, "foo");
    called++;
    return 42;
  }
};
assertEq("abcAbcABC".replace(myRegExp, "foo"), 42);
assertEq(called, 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
