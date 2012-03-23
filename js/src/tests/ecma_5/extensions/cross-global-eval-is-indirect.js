// |reftest| skip-if(!xulRuntime.shell) -- needs newGlobal()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 608473;
var summary =
  '|var eval = otherWindow.eval; eval(...)| should behave like indirectly ' +
  'calling that eval from a script in that other window';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var originalEval = eval;
var res;

function f()
{
  return [this, eval("this")];
}

var otherGlobalSameCompartment = newGlobal("same-compartment");

eval = otherGlobalSameCompartment.eval;
res = new f();
assertEq(res[0] !== res[1], true);
assertEq(res[0] !== this, true);
assertEq(res[0] instanceof f, true);
assertEq(res[1], otherGlobalSameCompartment);

res = f();
assertEq(res[0] !== res[1], true);
assertEq(res[0], this);
assertEq(res[1], otherGlobalSameCompartment);

var otherGlobalDifferentCompartment = newGlobal("new-compartment");

eval = otherGlobalDifferentCompartment.eval;
res = new f();
assertEq(res[0] !== res[1], true);
assertEq(res[0] !== this, true);
assertEq(res[0] instanceof f, true);
assertEq(res[1], otherGlobalDifferentCompartment);

res = f();
assertEq(res[0] !== res[1], true);
assertEq(res[0], this);
assertEq(res[1], otherGlobalDifferentCompartment);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
