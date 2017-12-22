// |reftest| skip-if(!xulRuntime.shell) -- needs evaluate()
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Check references to someVar, both as a variable and on |this|, in
// various evaluation contexts.
var someVar = 1;

// Top level.
assertEq(someVar, 1);
assertEq(this.someVar, 1);

// Inside evaluate.
evaluate("assertEq(someVar, 1);");
evaluate("assertEq(this.someVar, 1);");

// With an object on the scope, no shadowing.
var someObject = { someOtherField: 2 };
var evalOpt = { envChainObject: someObject };
evaluate("assertEq(someVar, 1);", evalOpt);
evaluate("assertEq(this.someVar, undefined);", evalOpt);

// With an object on the scope, shadowing global.
someObject = { someVar: 2 };
evalOpt = { envChainObject: someObject };
var alsoSomeObject = someObject;
evaluate("assertEq(someVar, 2);", evalOpt);
evaluate("assertEq(this.someVar, 2);", evalOpt);
evaluate("assertEq(this, alsoSomeObject);", evalOpt);

// With an object on the scope, inside a function.
evaluate("(function() { assertEq(someVar, 2);})()", evalOpt);
evaluate("(function() { assertEq(this !== alsoSomeObject, true);})()", evalOpt);
evaluate("(function() { assertEq(this.someVar, 1);})()", evalOpt);

var globalEvalOpt = {
    envChainObject: this
};
try {
  evaluate("assertEq(someVar, 1);", globalEvalOpt);
  throw new Error("Globals aren't allowed as a envChainObject argument to evaluate");
} catch (e) {
}


reportCompare(true, true);
