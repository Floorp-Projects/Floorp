/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = '15.2.3.7-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 430133;
var summary = 'ES5 Object.defineProperties(O, Properties)';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq("defineProperties" in Object, true);
assertEq(Object.defineProperties.length, 2);

var o, props, desc, passed;

o = {};
props =
  {
    a: { value: 17, enumerable: true, configurable: true, writable: true },
    b: { value: 42, enumerable: false, configurable: false, writable: false }
  };
Object.defineProperties(o, props);
assertEq("a" in o, true);
assertEq("b" in o, true);
desc = Object.getOwnPropertyDescriptor(o, "a");
assertEq(desc.value, 17);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);
desc = Object.getOwnPropertyDescriptor(o, "b");
assertEq(desc.value, 42);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.writable, false);

props =
  {
    c: { value: NaN, enumerable: false, configurable: true, writable: true },
    b: { value: 44 }
  };
var error = "before";
try
{
  Object.defineProperties(o, props);
  error = "no exception thrown";
}
catch (e)
{
  if (e instanceof TypeError)
    error = "typeerror";
  else
    error = "bad exception: " + e;
}
assertEq(error, "typeerror", "didn't throw or threw wrongly");
assertEq("c" in o, true, "new property added");
assertEq(o.b, 42, "old property value preserved");

function Properties() { }
Properties.prototype = { b: { value: 42, enumerable: true } };
props = new Properties();
Object.defineProperty(props, "a", { enumerable: false });
o = {};
Object.defineProperties(o, props);
assertEq("a" in o, false);
assertEq(Object.getOwnPropertyDescriptor(o, "a"), undefined,
         "Object.defineProperties(O, Properties) should only use enumerable " +
         "properties on Properties");
assertEq("b" in o, false);
assertEq(Object.getOwnPropertyDescriptor(o, "b"), undefined,
         "Object.defineProperties(O, Properties) should only use enumerable " +
         "properties directly on Properties");

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
