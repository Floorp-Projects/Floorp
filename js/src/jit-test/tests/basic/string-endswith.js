assertEq("abc".endsWith("abc"), true);
assertEq("abcd".endsWith("bcd"), true);
assertEq("abc".endsWith("c"), true);
assertEq("abc".endsWith("abcd"), false);
assertEq("abc".endsWith("bbc"), false);
assertEq("abc".endsWith("b"), false);
assertEq("abc".endsWith("abc", 3), true);
assertEq("abc".endsWith("bc", 3), true);
assertEq("abc".endsWith("a", 3), false);
assertEq("abc".endsWith("bc", 3), true);
assertEq("abc".endsWith("a", 1), true);
assertEq("abc".endsWith("abc", 1), false);
assertEq("abc".endsWith("b", 2), true);
assertEq("abc".endsWith("d", 2), false);
assertEq("abc".endsWith("dcd", 2), false);
assertEq("abc".endsWith("a", 42), false);
assertEq("abc".endsWith("bc", Infinity), true);
assertEq("abc".endsWith("a", Infinity), false);
assertEq("abc".endsWith("bc", undefined), true);
assertEq("abc".endsWith("bc", -43), false);
assertEq("abc".endsWith("bc", -Infinity), false);
assertEq("abc".endsWith("bc", NaN), false);
var myobj = {toString : (function () "abc"), endsWith : String.prototype.endsWith};
assertEq(myobj.endsWith("abc"), true);
assertEq(myobj.endsWith("ab"), false);
var gotStr = false, gotPos = false;
myobj = {toString : (function () {
    assertEq(gotPos, false);
    gotStr = true;
    return "xyz";
}),
endsWith : String.prototype.endsWith};
var idx = {valueOf : (function () {
    assertEq(gotStr, true);
    gotPos = true;
    return 42;
})};
myobj.endsWith("elephant", idx);
assertEq(gotPos, true);
