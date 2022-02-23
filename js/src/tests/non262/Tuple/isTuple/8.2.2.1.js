// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.2
The Tuple constructor:
...
has the following properties:
...

8.2.2.1 Tuple.isTuple ( arg )
The isTuple function takes one argument arg, and performs the following steps:

1. Return ! IsTuple(arg).
*/

var Tuple_isTuple = Tuple.isTuple;
assertEq(typeof Tuple_isTuple, "function");

assertEq(Tuple_isTuple(), false);
assertEq(Tuple_isTuple(Tuple.prototype), false);
assertEq(Tuple_isTuple([]), false);
assertEq(Tuple_isTuple(42), false);
assertEq(Tuple_isTuple(new Number(-50)), false);
assertEq(Tuple_isTuple(undefined), false);
assertEq(Tuple_isTuple(true), false);
assertEq(Tuple_isTuple(new Boolean(false)), false);
assertEq(Tuple_isTuple("hello"), false);
assertEq(Tuple_isTuple(new String("bye")), false);
assertEq(Tuple_isTuple({}), false);
assertEq(Tuple_isTuple(null), false);
assertEq(Tuple_isTuple(new RegExp()), false);
assertEq(Tuple_isTuple(JSON), false);
assertEq(Tuple_isTuple(Math), false);
assertEq(Tuple_isTuple(new Date()), false);
assertEq(Tuple_isTuple(new SyntaxError()), false);
var arg;
function fun() { arg = arguments; }(1, 2, 3);
assertEq(Tuple_isTuple(arg), false);
assertEq(Tuple_isTuple(this), false);
assertEq(Tuple_isTuple(function() {}), false);
var proto = Tuple.prototype;
var Con = function() {};
Con.prototype = proto;
var child = new Con();
assertEq(Tuple_isTuple(child), false);
assertEq(Tuple_isTuple({0: 1, 1: 2, length: 2}), false);

assertEq(Tuple_isTuple.call(1), false);
assertEq(Tuple_isTuple.call(#[1]), false);
assertEq(Tuple_isTuple.call(undefined, 1), false);
assertEq(Tuple_isTuple.call(undefined, undefined), false);
assertEq(Tuple_isTuple.call(undefined, #[1]), true);
assertEq(Tuple_isTuple.call(undefined, Object(#[1])), true);

reportCompare(0, 0);
