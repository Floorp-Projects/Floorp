
assertEq("foo".charAt(-123), "");
assertEq("foo".charAt(-1), "");
assertEq("foo".charAt(0), "f");
assertEq("foo".charAt(1), "o");
assertEq("foo".charAt(2), "o");
assertEq("foo".charAt(3.4), "");
assertEq("foo".charAt(), "f");
assertEq("".charAt(), "");
assertEq("".charAt(0), "");
assertEq("abc\u9123".charAt(3), "\u9123"); // char without unit string

/* Inferred as string.charAt(int). */
function charAt1(x) {
    return "abc".charAt(x);
}
assertEq(charAt1(-1), "");
assertEq(charAt1(0), "a");
assertEq(charAt1(1), "b");
assertEq(charAt1(2), "c");
assertEq(charAt1(3), "");
assertEq(charAt1(1234), "");

/* Inferred as string.charAt(double). */
function charAt2(x) {
    return "abc".charAt(x);
}
assertEq(charAt2(-1.3), "");
assertEq(charAt2(-0), "a");
assertEq(charAt2(2), "c");
assertEq(charAt2(2.3), "c");
assertEq(charAt2(3.14), "");
assertEq(charAt2(NaN), "a");

/* Test ropes. */
function charAt3(s, i) {
    var s2 = "abcdef" + s + "12345";
    return s2.charAt(i);
}
assertEq(charAt3("abcdef", 14), "3");
assertEq(charAt3("a" + "b", 1), "b");
assertEq(charAt3("abcdefg" + "hijklmnop", 10), "e");

/* Other 'this'. */
var arr = [1, 2];
arr.charAt = String.prototype.charAt;
assertEq(arr.charAt(1), ",");


