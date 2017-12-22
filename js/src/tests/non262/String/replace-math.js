/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 805121;
var summary = "Be more careful with string math to avoid wrong results";

print(BUGNUMBER + ": " + summary);

/******************************************************************************/

function puff(x, n)
{
  while(x.length < n)
    x += x;
  return x.substring(0, n);
}

var x = puff("1", 1 << 20);
var rep = puff("$1", 1 << 16);

try
{
  var y = x.replace(/(.+)/g, rep);
  assertEq(y.length, Math.pow(2, 36));
}
catch (e)
{
  // OOM also acceptable
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
