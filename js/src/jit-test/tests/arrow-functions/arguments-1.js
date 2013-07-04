// arrow functions have an 'arguments' binding, like any other function

var arguments = [];
var f = () => arguments;
var args = f();
assertEq(args.length, 0);
assertEq(Object.getPrototypeOf(args), Object.prototype);

args = f(true, false);
assertEq(args.length, 2);
assertEq(args[0], true);
assertEq(args[1], false);

