/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 1253099;
var summary =
  "RegExp.prototype.compile must perform all its steps *except* setting " +
  ".lastIndex, then throw, when provided a RegExp whose .lastIndex has been " +
  "made non-writable";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var regex = /foo/i;

// Aside from making .lastIndex non-writable, this has one incidental effect
// ubiquitously tested through the remainder of this test:
//
//   * RegExp.prototype.compile will do everything it ordinarily does, BUT it
//     will throw a TypeError when attempting to zero .lastIndex immediately
//     before succeeding overall.
//
// Ain't it great?
Object.defineProperty(regex, "lastIndex", { value: 42, writable: false });

assertEq(regex.global, false);
assertEq(regex.ignoreCase, true);
assertEq(regex.multiline, false);
assertEq(regex.unicode, false);
assertEq(regex.sticky, false);
assertEq(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assertEq(regex.lastIndex, 42);

assertEq(regex.test("foo"), true);
assertEq(regex.test("FOO"), true);
assertEq(regex.test("bar"), false);
assertEq(regex.test("BAR"), false);

assertThrowsInstanceOf(() => regex.compile("bar"), TypeError);

assertEq(regex.global, false);
assertEq(regex.ignoreCase, false);
assertEq(regex.multiline, false);
assertEq(regex.unicode, false);
assertEq(regex.sticky, false);
assertEq(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assertEq(regex.lastIndex, 42);
assertEq(regex.test("foo"), false);
assertEq(regex.test("FOO"), false);
assertEq(regex.test("bar"), true);
assertEq(regex.test("BAR"), false);

assertThrowsInstanceOf(() => regex.compile("^baz", "m"), TypeError);

assertEq(regex.global, false);
assertEq(regex.ignoreCase, false);
assertEq(regex.multiline, true);
assertEq(regex.unicode, false);
assertEq(regex.sticky, false);
assertEq(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assertEq(regex.lastIndex, 42);
assertEq(regex.test("foo"), false);
assertEq(regex.test("FOO"), false);
assertEq(regex.test("bar"), false);
assertEq(regex.test("BAR"), false);
assertEq(regex.test("baz"), true);
assertEq(regex.test("BAZ"), false);
assertEq(regex.test("012345678901234567890123456789012345678901baz"), false);
assertEq(regex.test("012345678901234567890123456789012345678901\nbaz"), true);
assertEq(regex.test("012345678901234567890123456789012345678901BAZ"), false);
assertEq(regex.test("012345678901234567890123456789012345678901\nBAZ"), false);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
