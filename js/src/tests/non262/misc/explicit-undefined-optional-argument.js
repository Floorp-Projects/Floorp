// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var BUGNUMBER = 373118;
var summary =
  'Properly handle explicitly-undefined optional arguments to a bunch of ' +
  'functions';

print(BUGNUMBER + ": " + summary);

//-----------------------------------------------------------------------------

var a;

a = "abc".slice(0, undefined);
assertEq(a, "abc");

a = "abc".substr(0, undefined);
assertEq(a, "abc");

a = "abc".substring(0, undefined);
assertEq(a, "abc");

a = [1, 2, 3].slice(0, undefined);
assertEq(a.join(), '1,2,3');

a = [1, 2, 3].sort(undefined);
assertEq(a.join(), '1,2,3');

assertEq((20).toString(undefined), '20');

//-----------------------------------------------------------------------------

reportCompare(true, true);
