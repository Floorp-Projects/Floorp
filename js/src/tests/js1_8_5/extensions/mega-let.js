// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 999999;
var summary =
  "Allow defining more than 2**16 let-variables in a single scope";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

print("Creating binding string...");

var bindings = [];
var codeStr = "let ";
for (var i = 0; i < Math.pow(2, 20); i++)
  bindings.push("x" + i + " = " + i);

var codeStr = "let " + bindings.join(", ") + ";";

print("Binding string created, testing with global eval...");

eval(codeStr);

print("Testing with Function...");

Function(codeStr)();

print("Testing inside a function...");

eval("function q() { " + codeStr + " }; q();");

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
