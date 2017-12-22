/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "eval-has-lexical-environment.js"
//-----------------------------------------------------------------------------
var BUGNUMBER = 1193583;
var summary =
  "Eval always has a lexical environment";

/**************
 * BEGIN TEST *
 **************/

eval(`
let foo = 42;
const kay = foo;
var bar = 84;
function f() {
  return foo + kay;
}
     `);

(1, eval)(`
let foo2 = 42;
const kay2 = foo2;
`);

// Lexical declarations should not have escaped eval.
assertEq(typeof foo, "undefined");
assertEq(typeof kay, "undefined");
assertEq(typeof foo2, "undefined");
assertEq(typeof kay2, "undefined");

// Eval'd functions can close over lexical bindings.
assertEq(f(), 84);

// Var can escape direct eval.
assertEq(bar, 84);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
