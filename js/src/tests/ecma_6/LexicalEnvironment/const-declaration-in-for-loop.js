/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "const-declaration-in-for-loop.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 1146644;
var summary =
  "Don't crash compiling a non-body-level for-loop whose loop declaration is " +
  "a const";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

for (const a1 = 3; false; )
  continue;

Function(`for (const a2 = 3; false; )
            continue;
         `)();

if (true)
{
  for (const a3 = 3; false; )
    continue;
}

Function(`if (true)
          {
            for (const a4 = 3; false; )
              continue;
          }`)();

// We don't support for (const ... in ...) or for (const ... of ...) yet.  When
// we do, these all should start passing without throwing a syntax error, and
// we can remove the try/catch here, and the ultimate throw-canary forcing this
// test to be updated.
try
{

evaluate(`for (const a5 of [])
            continue;`);

Function(`for (const a6 of [])
            continue;`)();

evaluate(`if (true)
          {
            for (const a7 of [])
              continue;
          }`);

Function(`if (true)
          {
            for (const a8 of [])
              continue;
          }`)();

evaluate(`for (const a9 in {})
            continue;`);

Function(`for (const a10 in {})
            continue;`)();

evaluate(`if (true)
          {
            for (const a11 in {})
              continue;
          }`);

Function(`if (true)
          {
            for (const a12 in {})
              continue;
          }`)();

throw new Error("Congratulations on making for (const … in/of …) work!  " +
                "Please remove the try/catch and this throw.");
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "unexpected error: expected SyntaxError, got " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
