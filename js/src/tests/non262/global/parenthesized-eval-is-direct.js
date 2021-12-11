// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
print("(eval)(...) is a direct eval, (1, eval)() isn't, etc.");

/**************
 * BEGIN TEST *
 **************/

/*
 * Justification:
 *
 *   https://mail.mozilla.org/pipermail/es5-discuss/2010-October/003724.html
 *
 * Note also bug 537673.
 */

var t = "global";

function group()
{
  var t = "local";
  return (eval)("t");
}
assertEq(group(), "local");

function groupAndComma()
{
  var t = "local";
  return (1, eval)("t");
}
assertEq(groupAndComma(), "global");

function groupAndTrueTernary()
{
  var t = "local";
  return (true ? eval : null)("t");
}
assertEq(groupAndTrueTernary(), "global");

function groupAndEmptyStringTernary()
{
  var t = "local";
  return ("" ? null : eval)("t");
}
assertEq(groupAndEmptyStringTernary(), "global");

function groupAndZeroTernary()
{
  var t = "local";
  return (0 ? null : eval)("t");
}
assertEq(groupAndZeroTernary(), "global");

function groupAndNaNTernary()
{
  var t = "local";
  return (0 / 0 ? null : eval)("t");
}
assertEq(groupAndNaNTernary(), "global");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
