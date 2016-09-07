/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 609756;
var summary =
  "Perform ToNumber on the result of the |fun()| in |fun()++| before " +
  "throwing";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var hadSideEffect;

function f()
{
  return { valueOf: function() { hadSideEffect = true; return 0; } };
}

hadSideEffect = false;
assertThrowsInstanceOf(function() { f()++; }, ReferenceError);
assertEq(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      f()++;
  }
}, ReferenceError);
assertEq(hadSideEffect, true);


hadSideEffect = false;
assertThrowsInstanceOf(function() { f()--; }, ReferenceError);
assertEq(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      f()--;
  }
}, ReferenceError);
assertEq(hadSideEffect, true);


hadSideEffect = false;
assertThrowsInstanceOf(function() { ++f(); }, ReferenceError);
assertEq(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      ++f();
  }
}, ReferenceError);
assertEq(hadSideEffect, true);


hadSideEffect = false;
assertThrowsInstanceOf(function() { --f(); }, ReferenceError);
assertEq(hadSideEffect, true);

hadSideEffect = false;
assertThrowsInstanceOf(function() {
  for (var i = 0; i < 20; i++)
  {
    if (i > 18)
      --f();
  }
}, ReferenceError);
assertEq(hadSideEffect, true);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
