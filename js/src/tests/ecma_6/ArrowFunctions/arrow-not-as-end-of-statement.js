/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Grab-bag of ArrowFunctions with block bodies appearing in contexts where the
// subsequent token-examination must use the Operand modifier to avoid an
// assertion.

assertEq(`x ${a => {}} z`, "x a => {} z");

for (; a => {}; )
  break;

for (; ; a => {})
  break;

switch (1)
{
  case a => {}:
   break;
}

try
{
  // Catch guards are non-standard, so ignore a syntax error.
  eval(`try
  {
  }
  catch (x if a => {})
  {
  }`);
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "should only have thrown SyntaxError, instead got " + e);
}

assertEq(0[a => {}], undefined);

class Y {};
class X extends Y { constructor() { super[a => {}](); } };

if (a => {})
  assertEq(true, true);
else
  assertEq(true, false);

switch (a => {})
{
}

with (a => {});

assertEq(typeof (a => {}), "function");

for (var x in y => {})
  continue;

var z = { x: 0 ? 1 : async a => {} };
assertEq(typeof z.x, "function");

var q = 0 ? 1 : async () => {};
assertEq(typeof q, "function");

if (typeof reportCompare === "function")
  reportCompare(true, true);
