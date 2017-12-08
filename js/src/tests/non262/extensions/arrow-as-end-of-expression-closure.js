// |reftest| skip-if(!xulRuntime.shell) -- needs getBuildConfiguration
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ArrowFunctions with block bodies appearing at the end of the
// AssignmentExpression returned by SpiderMonkey-specific function expression
// closures, where subsequent token-examination must use the Operand modifier
// to avoid an assertion.

if (getBuildConfiguration().release_or_beta)
{
  eval(`
  var ec1 = function() 0 ? 1 : a => {};
  assertEq(typeof ec1, "function");
  assertEq(typeof ec1(), "function");
  assertEq(ec1()(), undefined);

  var ec2 = function() 0 ? 1 : a => {} // deliberately exercise ASI here
  assertEq(typeof ec2, "function");
  assertEq(typeof ec2(), "function");
  assertEq(ec2()(), undefined);

  function ec3() 0 ? 1 : a => {} // exercise ASI here
  assertEq(typeof ec3(), "function");

  function ec4() 0 ? 1 : a => {};
  assertEq(typeof ec4(), "function");

  var needle = "@";
  var x = 42;
  var g = { test() { assertEq(true, false, "shouldn't be called"); } };

  function ec5() 0 ? 1 : a => {} // ASI
  /x/g.test((needle = "x"));
  assertEq(needle, "x");
  `);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
