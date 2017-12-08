// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 657367;
var summary = "eval must not parse strings containing U+2028 or U+2029";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function esc(s)
{
  return s.split("").map(function(v)
                         {
                           var code =
                             ("000" + v.charCodeAt(0).toString(16)).slice(-4);
                           return "\\u" + code;
                         }).join("");
}

try
{
  var r = eval('"\u2028"');
  throw new Error("\"\\u2028\" didn't throw, returned " + esc(r));
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "U+2028 is not a valid string character");
}

try
{
  var r = eval('("\u2028")');
  throw new Error("(\"\\u2028\") didn't throw, returned " + esc(r));
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "U+2028 is not a valid string character");
}

try
{
  var r = eval('"\u2029"');
  throw new Error("\"\\u2029\" didn't throw, returned " + esc(r));
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "U+2029 is not a valid string character");
}

try
{
  var r = eval('("\u2029")');
  throw new Error("(\"\\u2029\") didn't throw, returned " + esc(r));
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "U+2029 is not a valid string character");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");
