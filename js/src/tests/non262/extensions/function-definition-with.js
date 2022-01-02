// |reftest| skip-if(!xulRuntime.shell) -- needs evaluate()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 577325;
var summary = 'Implement the ES5 algorithm for processing function statements';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var called, obj;

function inFile1() { return "in file"; }
called = false;
obj = { set inFile1(v) { called = true; } };
with (obj) {
  function inFile1() { return "in file in with"; };
}
assertEq(inFile1(), "in file in with");
assertEq("set" in Object.getOwnPropertyDescriptor(obj, "inFile1"), true);
assertEq(called, false);

evaluate("function notInFile1() { return 'not in file'; }");
called = false;
obj = { set notInFile1(v) { called = true; return "not in file 2"; } };
with (obj) {
  function notInFile1() { return "not in file in with"; };
}
assertEq(notInFile1(), "not in file in with");
assertEq("set" in Object.getOwnPropertyDescriptor(obj, "notInFile1"), true);
assertEq(called, false);

function inFile2() { return "in file 1"; }
called = false;
obj =
  Object.defineProperty({}, "inFile2",
                        { value: 42, configurable: false, enumerable: false });
with (obj) {
  function inFile2() { return "in file 2"; };
}
assertEq(inFile2(), "in file 2");
assertEq(obj.inFile2, 42);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
