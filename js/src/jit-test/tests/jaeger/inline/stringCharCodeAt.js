
assertEq("foo".charCodeAt(-123), NaN);
assertEq("foo".charCodeAt(-0), 102);
assertEq("foo".charCodeAt(0), 102);
assertEq("foo".charCodeAt(2), 111);
assertEq("foo".charCodeAt(3.4), NaN);
assertEq("foo".charCodeAt(), 102);
assertEq("".charCodeAt(), NaN);
assertEq("".charCodeAt(0), NaN);

/* Inferred as string.charCodeAt(int). */
function charCodeAt1(x) {
    return "abc".charCodeAt(x);
}
assertEq(charCodeAt1(-1), NaN);
assertEq(charCodeAt1(0), 97);
assertEq(charCodeAt1(1), 98);
assertEq(charCodeAt1(2), 99);
assertEq(charCodeAt1(3), NaN);
assertEq(charCodeAt1(1234), NaN);

/* Inferred as string.charCodeAt(double). */
function charCodeAt2(x) {
    return "abc".charCodeAt(x);
}
assertEq(charCodeAt2(-1.3), NaN);
assertEq(charCodeAt2(-0), 97);
assertEq(charCodeAt2(2), 99);
assertEq(charCodeAt2(2.3), 99);
assertEq(charCodeAt2(3.14), NaN);
assertEq(charCodeAt2(NaN), 97);

/* Test ropes. */
function charCodeAt3(s, i) {
    var s2 = "abcdef" + s + "12345";
    return s2.charCodeAt(i);
}
assertEq(charCodeAt3("abcdef", 14), 51);
assertEq(charCodeAt3("a" + "b", 1), 98);
assertEq(charCodeAt3("abcdefg" + "hijklmnop", 10), 101);

/* Other 'this'. */
var n = new Number(123);
n.charCodeAt = String.prototype.charCodeAt;
assertEq(n.charCodeAt(1), 50);


