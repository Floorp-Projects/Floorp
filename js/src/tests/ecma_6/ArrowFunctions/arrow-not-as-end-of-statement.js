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

assertEq(eval("a => {}, 17, 42;"), 42);
assertEq(eval("42, a => {}, 17;"), 17);
assertEq(typeof eval("17, 42, a => {};"), "function");

assertEq(eval("1 ? 0 : a => {}, 17, 42;"), 42);
assertEq(eval("42, 1 ? 0 : a => {}, 17;"), 17);
assertEq(eval("17, 42, 1 ? 0 : a => {};"), 0);

var z = { x: 0 ? 1 : async a => {} };
assertEq(typeof z.x, "function");

var q = 0 ? 1 : async () => {};
assertEq(typeof q, "function");

var m = 0 ? 42 : m = foo => {} // ASI
/Q/g;
assertEq(typeof m, "function");

var { q: w = 0 ? 1 : a => {} } = {};
assertEq(typeof w, "function");

Function.prototype.c = 42;
var { c } = 0 ? 1 : a => {} // ASI
/Q/g;
assertEq(c, 42);

var c = 0 ? 1 : a => {}
/Q/g;
assertEq(typeof c, "function");
delete Function.prototype.c;

assertEq(typeof eval(0 ? 1 : a => {}), "function");

var zoom = 1 ? a => {} : 357;
assertEq(typeof zoom, "function");

var { b = 0 ? 1 : a => {} } = {};
assertEq(typeof b, "function");

var [k = 0 ? 1 : a => {}] = [];
assertEq(typeof k, "function");

assertEq(typeof [0 ? 1 : a => {}][0], "function");

var { [0 ? 1 : a => {}]: h } = { "a => {}": "boo-urns!" };
assertEq(h, "boo-urns!");

if (typeof reportCompare === "function")
  reportCompare(true, true);
