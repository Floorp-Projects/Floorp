/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 721322;
var summary =
  'f.arguments must trigger an arguments object in non-strict mode functions';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var obj =
  {
    test: function()
    {
      var args = obj.test.arguments;
      assertEq(args !== null, true);
      assertEq(args[0], 5);
      assertEq(args[1], undefined);
      assertEq(args.length, 2);
    }
  };
obj.test(5, undefined);

var sobj =
  {
    test: function()
    {
     "use strict";

      try
      {
        var args = sobj.test.arguments;
        throw new Error("access to arguments property of strict mode " +
                        "function didn't throw");
      }
      catch (e)
      {
        assertEq(e instanceof TypeError, true,
                 "should have thrown TypeError, instead got: " + e);
      }
    }
  };
sobj.test(5, undefined);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
