/* Test String.prototype.repeat */

load(libdir + 'asserts.js');

assertEq("abc".repeat(1), "abc");
assertEq("abc".repeat(2), "abcabc");
assertEq("abc".repeat(3), "abcabcabc");
assertEq("a".repeat(10), "aaaaaaaaaa");
var myobj = {toString : (function () "abc"), repeat : String.prototype.repeat};
assertEq(myobj.repeat(1), "abc");
assertEq(myobj.repeat(2), "abcabc");
assertEq("abc".repeat(0), "");
assertThrowsInstanceOf(function() {
                         "abc".repeat(-1);
                       }, RangeError,
                       "String.prototype.repeat should raise RangeError on negative arguments");
assertThrowsInstanceOf(function() {
                         "abc".repeat(Number.POSITIVE_INFINITY);
                       }, RangeError,
                       "String.prototype.repeat should raise RangeError on positive infinity arguments");
assertEq("".repeat(5), "");
assertEq("abc".repeat(2.0), "abcabc");
