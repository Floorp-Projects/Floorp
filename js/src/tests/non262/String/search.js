var BUGNUMBER = 887016;
var summary = "Call RegExp.prototype[@@search] from String.prototype.search.";

print(BUGNUMBER + ": " + summary);

var called = 0;
var myRegExp = {
  [Symbol.search](S) {
    assertEq(S, "abcAbcABC");
    called++;
    return 42;
  }
};
assertEq("abcAbcABC".search(myRegExp), 42);
assertEq(called, 1);

called = 0;
RegExp.prototype[Symbol.search] = function(S) {
  assertEq(S, "abcAbcABC");
  called++;
  return 43;
};
assertEq("abcAbcABC".search("abc"), 43);
assertEq(called, 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
