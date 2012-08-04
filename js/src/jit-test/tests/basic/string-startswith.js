assertEq("abc".startsWith("abc"), true);
assertEq("abcd".startsWith("abc"), true);
assertEq("abc".startsWith("a"), true);
assertEq("abc".startsWith("abcd"), false);
assertEq("abc".startsWith("bcde"), false);
assertEq("abc".startsWith("b"), false);
assertEq("abc".startsWith("abc", 0), true);
assertEq("abc".startsWith("bc", 0), false);
assertEq("abc".startsWith("bc", 1), true);
assertEq("abc".startsWith("c", 1), false);
assertEq("abc".startsWith("abc", 1), false);
assertEq("abc".startsWith("c", 2), true);
assertEq("abc".startsWith("d", 2), false);
assertEq("abc".startsWith("dcd", 2), false);
assertEq("abc".startsWith("a", 42), false);
assertEq("abc".startsWith("a", Infinity), false);
assertEq("abc".startsWith("a", NaN), true);
assertEq("abc".startsWith("b", NaN), false);
assertEq("abc".startsWith("ab", -43), true);
assertEq("abc".startsWith("ab", -Infinity), true);
assertEq("abc".startsWith("bc", -42), false);
assertEq("abc".startsWith("bc", -Infinity), false);
var myobj = {toString : (function () "abc"), startsWith : String.prototype.startsWith};
assertEq(myobj.startsWith("abc"), true);
assertEq(myobj.startsWith("bc"), false);
var gotStr = false, gotPos = false;
myobj = {toString : (function () {
    assertEq(gotPos, false);
    gotStr = true;
    return "xyz";
}),
startsWith : String.prototype.startsWith};
var idx = {valueOf : (function () {
    assertEq(gotStr, true);
    gotPos = true;
    return 42;
})};
myobj.startsWith("elephant", idx);
assertEq(gotPos, true);
