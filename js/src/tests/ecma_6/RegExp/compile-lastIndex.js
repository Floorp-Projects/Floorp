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

// Aside from making .lastIndex non-writable, this has two incidental effects
// ubiquitously tested through the remainder of this test:
//
//   * RegExp.prototype.compile will do everything it ordinarily does, BUT it
//     will throw a TypeError when attempting to zero .lastIndex immediately
//     before succeeding overall.
//   * RegExp.prototype.test for a non-global and non-sticky regular expression,
//     in case of a match, will return true (as normal).  BUT if no match is
//     found, it will throw a TypeError when attempting to modify .lastIndex.
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
assertThrowsInstanceOf(() => regex.test("bar"), TypeError);
assertThrowsInstanceOf(() => regex.test("BAR"), TypeError);

assertThrowsInstanceOf(() => regex.compile("bar"), TypeError);

assertEq(regex.global, false);
assertEq(regex.ignoreCase, false);
assertEq(regex.multiline, false);
assertEq(regex.unicode, false);
assertEq(regex.sticky, false);
assertEq(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assertEq(regex.lastIndex, 42);
assertThrowsInstanceOf(() => regex.test("foo"), TypeError);
assertThrowsInstanceOf(() => regex.test("FOO"), TypeError);
assertEq(regex.test("bar"), true);
assertThrowsInstanceOf(() => regex.test("BAR"), TypeError);

assertThrowsInstanceOf(() => regex.compile("^baz", "m"), TypeError);

assertEq(regex.global, false);
assertEq(regex.ignoreCase, false);
assertEq(regex.multiline, true);
assertEq(regex.unicode, false);
assertEq(regex.sticky, false);
assertEq(Object.getOwnPropertyDescriptor(regex, "lastIndex").writable, false);
assertEq(regex.lastIndex, 42);
assertThrowsInstanceOf(() => regex.test("foo"), TypeError);
assertThrowsInstanceOf(() => regex.test("FOO"), TypeError);
assertThrowsInstanceOf(() => regex.test("bar"), TypeError);
assertThrowsInstanceOf(() => regex.test("BAR"), TypeError);
assertEq(regex.test("baz"), true);
assertThrowsInstanceOf(() => regex.test("BAZ"), TypeError);
assertThrowsInstanceOf(() => regex.test("012345678901234567890123456789012345678901baz"),
                       TypeError);
assertEq(regex.test("012345678901234567890123456789012345678901\nbaz"), true);
assertThrowsInstanceOf(() => regex.test("012345678901234567890123456789012345678901BAZ"),
                       TypeError);
assertThrowsInstanceOf(() => regex.test("012345678901234567890123456789012345678901\nBAZ"),
                       TypeError);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
