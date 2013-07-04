// 'arguments' in arrow functions nested in other functions

var g;
function f() {
    g = () => arguments;
}
f();
var args = g();
assertEq(args.length, 0);

args = g(1, 2, 3);
assertEq(args.length, 3);
assertEq(args[0], 1);
assertEq(args[2], 3);
