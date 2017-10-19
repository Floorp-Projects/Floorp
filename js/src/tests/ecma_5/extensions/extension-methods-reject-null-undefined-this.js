/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 619283;
var summary =
  "ECMAScript built-in methods that immediately throw when |this| is " +
  "|undefined| or |null| (due to CheckObjectCoercible, ToObject, or ToString)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// This test fills out for the non-standard methods which
// ecma_5/misc/builtin-methods-reject-null-undefined-this.js declines to test.

var ClassToMethodMap =
  {
    Object:   [/*
                * Don't box this just yet for these methods -- they're used too
                * much without qualification to do that.  :-(
                */
               /* "__defineGetter__", "__defineSetter__", */
               "__lookupGetter__", "__lookupSetter__", "watch", "unwatch",
               "toSource"],
    Function: ["toSource"],
    Array:    ["toSource"],
    String:   ["toSource", "quote", "bold", "italics", "fixed", "fontsize",
               "fontcolor", "link", "anchor", "strike", "small", "big", "blink",
               "sup", "sub", "substr", "trimLeft", "trimRight", "toJSON"],
    Boolean:  ["toSource", "toJSON"],
    Number:   ["toSource", "toJSON"],
    Date:     ["toSource", "getYear", "setYear",  "toGMTString"],
    RegExp:   ["toSource"],
    Error:    ["toSource"],
  };

var badThisValues = [null, undefined];

function testMethod(Class, className, method)
{
  var expr;

  // Try out explicit this values
  for (var i = 0, sz = badThisValues.length; i < sz; i++)
  {
    var badThis = badThisValues[i];

    expr = className + ".prototype." + method + ".call(" + badThis + ")";
    try
    {
      Class.prototype[method].call(badThis);
      throw new Error(expr + " didn't throw a TypeError");
    }
    catch (e)
    {
      assertEq(e instanceof TypeError, true,
               "wrong error for " + expr + ", instead threw " + e);
    }

    expr = className + ".prototype." + method + ".apply(" + badThis + ")";
    try
    {
      Class.prototype[method].apply(badThis);
      throw new Error(expr + " didn't throw a TypeError");
    }
    catch (e)
    {
      assertEq(e instanceof TypeError, true,
               "wrong error for " + expr + ", instead threw " + e);
    }
  }

  // ..and for good measure..

  expr = "(0, " + className + ".prototype." + method + ")()"
  try
  {
    // comma operator to call GetValue() on the method and de-Reference it
    (0, Class.prototype[method])();
    throw new Error(expr + " didn't throw a TypeError");
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "wrong error for " + expr + ", instead threw " + e);
  }
}

for (var className in ClassToMethodMap)
{
  var Class = this[className];

  var methodNames = ClassToMethodMap[className];
  for (var i = 0, sz = methodNames.length; i < sz; i++)
  {
    var method = methodNames[i];
    testMethod(Class, className, method);
  }
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
