// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1199695;
var summary =
  "Computed property names must be considered as always effectful even when " +
  "the name expression isn't effectful, because calling ToPropertyKey on " +
  "some non-effectful expressions has user-modifiable behavior";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

RegExp.prototype.toString = () => { throw 42; };
assertThrowsValue(function() {
  ({ [/regex/]: 0 }); // ToPropertyKey(/regex/) throws 42
}, 42);

function Q() {
  ({ [new.target]: 0 }); // new.target will be Q, ToPropertyKey(Q) throws 17
}
Q.toString = () => { throw 17; };
assertThrowsValue(function() {
  new Q;
}, 17);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
