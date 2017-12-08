/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var errors = ["Error", "EvalError", "RangeError", "ReferenceError",
              "SyntaxError", "TypeError", "URIError"];

for (var i = 0; i < errors.length; i++) {
  var error = this[errors[i]];

  var desc = Object.getOwnPropertyDescriptor(error.prototype, "name");
  assertEq(!!desc, true, errors[i] + ".prototype.name should exist");
  assertEq((desc || {}).enumerable, false, errors[i] + ".prototype.name should not be enumerable");

  desc = Object.getOwnPropertyDescriptor(error.prototype, "message");
  assertEq(!!desc, true, errors[i] + ".prototype.message should exist");
  assertEq((desc || {}).enumerable, false, errors[i] + ".prototype.message should not be enumerable");

  var instance = new error;
  desc = Object.getOwnPropertyDescriptor(instance, "message");
  assertEq(!!desc, false, "new " + errors[i] + ".message should not exist");

  instance = new error("a message");
  desc = Object.getOwnPropertyDescriptor(instance, "message");
  assertEq(!!desc, true, "new " + errors[i] + "(...).message should exist");
  assertEq((desc || {}).enumerable, false, "new " + errors[i] + "(...).message should not be enumerable");
}

reportCompare(true, true);
