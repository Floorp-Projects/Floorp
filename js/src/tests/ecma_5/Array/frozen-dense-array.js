/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Author: Emilio Cobos Álvarez <ecoal95@gmail.com>
 */
var BUGNUMBER = 1310744;
var summary = "Dense array properties shouldn't be modified when they're frozen";

print(BUGNUMBER + ": " + summary);

var a = Object.freeze([4, 5, 1]);

function assertArrayIsExpected() {
  assertEq(a.length, 3);
  assertEq(a[0], 4);
  assertEq(a[1], 5);
  assertEq(a[2], 1);
}

assertThrowsInstanceOf(() => a.reverse(), TypeError);
assertThrowsInstanceOf(() => a.shift(), TypeError);
assertThrowsInstanceOf(() => a.unshift(0), TypeError);
assertThrowsInstanceOf(() => a.sort(function() {}), TypeError);
assertThrowsInstanceOf(() => a.pop(), TypeError);
assertThrowsInstanceOf(() => a.fill(0), TypeError);
assertThrowsInstanceOf(() => a.splice(0, 1, 1), TypeError);
assertThrowsInstanceOf(() => a.push("foo"), TypeError);
assertThrowsInstanceOf(() => { "use strict"; a.length = 5; }, TypeError);
assertThrowsInstanceOf(() => { "use strict"; a[2] = "foo"; }, TypeError);
assertThrowsInstanceOf(() => { "use strict"; delete a[0]; }, TypeError);
assertThrowsInstanceOf(() => a.splice(Math.a), TypeError);

// Shouldn't throw, since this is not strict mode, but shouldn't change the
// value of the property.
a.length = 5;
a[2] = "foo";
assertEq(delete a[0], false);

assertArrayIsExpected();

var watchpointCalled = false;
// NOTE: Be careful with the position of this test, since this sparsifies the
// elements and you might not test what you think you're testing otherwise.
a.watch(2, function(prop, oldValue, newValue) {
  watchpointCalled = true;
  assertEq(prop, 2);
  assertEq(oldValue, 1);
  assertEq(newValue, "foo");
});

assertArrayIsExpected();

a.length = 5;
a[2] = "foo";
assertEq(watchpointCalled, true);
assertEq(delete a[0], false);

assertArrayIsExpected();

if (typeof reportCompare === "function")
  reportCompare(true, true);
