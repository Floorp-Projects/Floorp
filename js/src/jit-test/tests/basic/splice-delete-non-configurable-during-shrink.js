/* Test that splice causing deletion of a non-configurable property stops at exactly step 12(v) of ES5 15.4.4.12 */

var O = [1,2,3,4,5,6];
var A = undefined;
Object.defineProperty(O, 3, { configurable: false });

try
{
  A = O.splice(0, 6);
  throw new Error("didn't throw, returned " + A);
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "deleting O[3] should have caused a TypeError");
}

assertEq(O.length, 6); // setting length not reached
assertEq(A, undefined); // return value not reached

assertEq(O[5], undefined); // deletion reached
assertEq(O[4], undefined); // deletion reached
assertEq(O[3], 4); // deletion caused exception
assertEq(O[2], 3); // deletion not reached
assertEq(O[1], 2); // deletion not reached
assertEq(O[0], 1); // deletion not reached
