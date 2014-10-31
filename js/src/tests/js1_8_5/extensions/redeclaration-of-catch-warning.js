// |reftest| skip-if(!xulRuntime.shell)
//
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 622646;
var summary = "Shadowing an exception identifier in a catch block with a " +
              "|const| or |let| declaration should throw an error";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

options("strict");

function assertRedeclarationErrorThrown(expression)
{
  try
  {
    evaluate(expression);
    throw new Error("Redeclaration error wasn't thrown");
  }
  catch(e)
  {
    assertEq(e.message.indexOf("catch") > 0, true,
             "wrong error, got " + e.message);
  }
}

assertRedeclarationErrorThrown("try {} catch(e) { const e; }");
assertRedeclarationErrorThrown("try {} catch(e) { let e; }");

if (typeof reportCompare === "function")
  reportCompare(true, true);
