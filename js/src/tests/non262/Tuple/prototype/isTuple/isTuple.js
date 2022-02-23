// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var f = Tuple.isTuple;

assertEq(typeof f, "function");
assertEq(f.length, 1);
assertEq(f(#[]), true);
assertEq(f(#[1]), true);
assertEq(f(#[1,2,3]), true);
assertEq(f(Object(#[])), true);
assertEq(f(Object(#[1])), true);
assertEq(f(Object(#[1,2,3])), true);

for (thing of [42, new Number(-42), undefined, true, false, "abc" , new String("a\nb\\!"), {}, [], [1,2,3], new Uint8Array(1,2,3), null, new RegExp(), JSON, new SyntaxError(), function() {}, Math, new Date()]) {
    assertEq(f(thing), false);
}
assertEq(f(Tuple.prototype), false);
var arg;
(function fun() { arg = arguments; }(1,2,3));
assertEq(f(arg), false);
assertEq(f(this), false);

var proto = [];
var Con = function() {};
Con.prototype = proto;

var child = new Con();

assertEq(f(child), false);

var proto = Tuple.prototype;
var Con = function() {};
Con.prototype = proto;

var child = new Con();

assertEq(f(child), false);

assertEq(f({
  0: 12,
  1: 9,
  length: 2
}), false);


reportCompare(0, 0);


