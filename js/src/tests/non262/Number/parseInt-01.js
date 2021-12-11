// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 886949;
var summary = "ES6 (draft May 2013) 15.7.3.9 Number.parseInt(string, radix)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var str, radix;
var upvar;

/* 1. Let inputString be ToString(string). */

assertEq(Number.parseInt({ toString: function() { return "17" } }, 10), 17);

upvar = 0;
str = { get toString() { upvar++; return function() { upvar++; return "12345"; } } };
assertEq(Number.parseInt(str, 10), 12345);
assertEq(upvar, 2);


/*
 * 2. Let S be a newly created substring of inputString consisting of the first
 *    character that is not a StrWhiteSpaceChar and all characters following
 *    that character. (In other words, remove leading white space.)
 */

var ws =
  ["\t", "\v", "\f", " ", "\xA0", "\uFEFF",
     "\u2004", "\u3000", // a few Unicode whitespaces
   "\r", "\n", "\u2028", "\u2029"];

str = "8675309";
for (var i = 0, sz = ws.length; i < sz; i++)
{
  assertEq(Number.parseInt(ws[i] + str, 10), 8675309);
  for (var j = 0, sz = ws.length; j < sz; j++)
  {
    assertEq(Number.parseInt(ws[i] + ws[j] + str, 10), 8675309,
             ws[i].charCodeAt(0).toString(16) + ", " +
             ws[j].charCodeAt(0).toString(16));
  }
}


/*
 * 3. Let sign be 1.
 * 4. If S is not empty and the first character of S is a minus sign -, let
 *    sign be −1.
 */
str = "5552368";
assertEq(Number.parseInt("-" + str, 10), -Number.parseInt(str, 10));
assertEq(Number.parseInt(" -" + str, 10), -Number.parseInt(str, 10));
assertEq(Number.parseInt("-", 10), NaN);
assertEq(Number.parseInt("", 10), NaN);
assertEq(Number.parseInt("-0", 10), -0);


/*
 * 5. If S is not empty and the first character of S is a plus sign + or a
 *    minus sign -, then remove the first character from S.
 */
assertEq(Number.parseInt("+12345", 10), 12345);
assertEq(Number.parseInt(" +12345", 10), 12345);
assertEq(Number.parseInt("-12345", 10), -12345);
assertEq(Number.parseInt(" -12345", 10), -12345);


/*
 * 6.  Let R = ToInt32(radix).
 */

upvar = "";
str =
  { toString: function() { if (!upvar) upvar = "string"; return "42"; } };
radix =
  { toString: function() { if (!upvar) upvar = "radix"; return "10"; } };

assertEq(Number.parseInt(str, radix), 42);
assertEq(upvar, "string");

assertEq(Number.parseInt("123", null), 123);
assertEq(Number.parseInt("123", undefined), 123);
assertEq(Number.parseInt("123", NaN), 123);
assertEq(Number.parseInt("123", -0), 123);
assertEq(Number.parseInt("10", 72057594037927950), 16);
assertEq(Number.parseInt("10", -4294967292), 4);
assertEq(Number.parseInt("0x10", 1e308), 16);
assertEq(Number.parseInt("10", 1e308), 10);
assertEq(Number.parseInt("10", { valueOf: function() { return 16; } }), 16);


/*
 * 7.  Let stripPrefix be true.
 * 8.  If R ≠ 0, then
 *     a. If R < 2 or R > 36, then return NaN.
 *     b. If R ≠ 16, let stripPrefix be false.
 * 9.  Else, R = 0
 *     a. Let R = 10.
 * 10. If stripPrefix is true, then
 *     a. If the length of S is at least 2 and the first two characters of S
 *     are either “0x” or “0X”, then remove the first two characters from S and
 *     let R = 16.
 */
var vs = ["1", "51", "917", "2343", "99963"];
for (var i = 0, sz = vs.length; i < sz; i++)
  assertEq(Number.parseInt(vs[i], 0), Number.parseInt(vs[i], 10), "bad " + vs[i]);

assertEq(Number.parseInt("0x10"), 16);
assertEq(Number.parseInt("0x10", 0), 16);
assertEq(Number.parseInt("0x10", 16), 16);
assertEq(Number.parseInt("0x10", 8), 0);
assertEq(Number.parseInt("-0x10", 16), -16);

assertEq(Number.parseInt("5", 1), NaN);
assertEq(Number.parseInt("5", 37), NaN);
assertEq(Number.parseInt("5", { valueOf: function() { return -1; } }), NaN);


/*
 * 11. If S contains any character that is not a radix-R digit, then let Z be
 *     the substring of S consisting of all characters before the first such
 *     character; otherwise, let Z be S.
 * 12. If Z is empty, return NaN.
 */
assertEq(Number.parseInt(""), NaN);
assertEq(Number.parseInt("ohai"), NaN);
assertEq(Number.parseInt("0xohai"), NaN);
assertEq(Number.parseInt("-ohai"), NaN);
assertEq(Number.parseInt("+ohai"), NaN);
assertEq(Number.parseInt(" ohai"), NaN);

assertEq(Number.parseInt("0xaohai"), 10);
assertEq(Number.parseInt("hohai", 18), 17);


/*
 * 13. Let mathInt be the mathematical integer value that is represented by Z
 *     in radix-R notation, using the letters A-Z and a-z for digits with
 *     values 10 through 35. (However, if R is 10 and Z contains more than 20
 *     significant digits, every significant digit after the 20th may be
 *     replaced by a 0 digit, at the option of the implementation; and if R is
 *     not 2, 4, 8, 10, 16, or 32, then mathInt may be an implementation-
 *     dependent approximation to the mathematical integer value that is
 *     represented by Z in radix-R notation.)
 * 14. Let number be the Number value for mathInt.
 * 15. Return sign × number.
 */
assertEq(Number.parseInt("ohai", 36), 1142154);
assertEq(Number.parseInt("0ohai", 36), 1142154);
assertEq(Number.parseInt("00ohai", 36), 1142154);
assertEq(Number.parseInt("A", 16), 10);
assertEq(Number.parseInt("0A", 16), 10);
assertEq(Number.parseInt("00A", 16), 10);
assertEq(Number.parseInt("A", 17), 10);
assertEq(Number.parseInt("0A", 17), 10);
assertEq(Number.parseInt("00A", 17), 10);

/* Number.parseInt should be the same function object as global parseInt. */
assertEq(Number.parseInt, parseInt);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
