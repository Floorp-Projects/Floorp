/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

//-----------------------------------------------------------------------------
var BUGNUMBER = 730831;
var summary = 'Date.prototype.toISOString returns an invalid ISO-8601 string';

print(BUGNUMBER + ": " + summary);

function iso(t) {
  return new Date(t).toISOString();
}

function utc(year, month, day, hour, minute, second, millis) {
  var date = new Date(0);
  date.setUTCFullYear(year, month - 1, day);
  date.setUTCHours(hour, minute, second, millis);
  return date.getTime();
}


// Values around maximum date for simple iso format.
var maxDateSimple = utc(9999, 12, 31, 23, 59, 59, 999);
assertEq(iso(maxDateSimple - 1),    "9999-12-31T23:59:59.998Z");
assertEq(iso(maxDateSimple    ),    "9999-12-31T23:59:59.999Z");
assertEq(iso(maxDateSimple + 1), "+010000-01-01T00:00:00.000Z");


// Values around minimum date for simple iso format.
var minDateSimple = utc(0, 1, 1, 0, 0, 0, 0);
assertEq(iso(minDateSimple - 1), "-000001-12-31T23:59:59.999Z");
assertEq(iso(minDateSimple    ),    "0000-01-01T00:00:00.000Z");
assertEq(iso(minDateSimple + 1),    "0000-01-01T00:00:00.001Z");


// Values around maximum date for extended iso format.
var maxDateExtended = utc(+275760, 9, 13, 0, 0, 0, 0);
assertEq(maxDateExtended, +8.64e15);
assertEq(iso(maxDateExtended - 1), "+275760-09-12T23:59:59.999Z");
assertEq(iso(maxDateExtended    ), "+275760-09-13T00:00:00.000Z");
assertThrowsInstanceOf(() => iso(maxDateExtended + 1), RangeError);


// Values around minimum date for extended iso format.
var minDateExtended = utc(-271821, 4, 20, 0, 0, 0, 0);
assertEq(minDateExtended, -8.64e15);
assertThrowsInstanceOf(() => iso(minDateExtended - 1), RangeError);
assertEq(iso(minDateExtended    ), "-271821-04-20T00:00:00.000Z");
assertEq(iso(minDateExtended + 1), "-271821-04-20T00:00:00.001Z");


if (typeof reportCompare === "function")
  reportCompare(true, true);
