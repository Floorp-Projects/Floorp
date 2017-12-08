// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1199546;
var summary =
  "Convert computed property name expressions to property key before " +
  "evaluating the property's value";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var s = "foo";
var convertsToS = { toString() { return s; } };

var o = {
  [convertsToS]: // after ToPropertyKey becomes "foo"
    (function() {
      s = 'bar';
      return 'abc'; // so we have "foo": "bar" for the first property
     })(),

  [convertsToS]: // |s| was set above to "bar", so after ToPropertyKey, "bar"
    'efg' // so we have "bar": "efg" for the second property
};

assertEq(o.foo, "abc");
assertEq(o.bar, "efg");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
