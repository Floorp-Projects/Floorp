// Make sure we properly update the getter when we update the holder
// shape in the getter IC.
function f(obj) {
    var x;
    for (var i = 0; i < 20; ++i) {
	x = obj.foo;
    }
    return x;
}

var proto = {};
var obj1 = Object.create(proto);
var obj2 = Object.create(proto);
obj2.bar = "5";
Object.defineProperty(proto, "foo",
		      { get: function() { return 1; }, configurable: true });
assertEq(f(obj1), 1);
assertEq(f(obj2), 1);

Object.defineProperty(proto, "foo",
		      { get: function() { return 2; }, configurable: true });
assertEq(f(obj1), 2);
assertEq(f(obj2), 2);

// Make sure we properly update the setter when we update the holder
// shape in the setter IC.
function g(obj) {
    var x;
    for (var i = 0; i < 20; ++i) {
	obj.foo = i;
    }
    return x;
}

var proto = {};
var obj1 = Object.create(proto);
var obj2 = Object.create(proto);
var sideEffect;
obj2.bar = "5";
Object.defineProperty(proto, "foo",
		      { set: function() { sideEffect = 1; }, configurable: true });
g(obj1);
assertEq(sideEffect, 1);
sideEffect = undefined;
g(obj2);
assertEq(sideEffect, 1);
sideEffect = undefined;

Object.defineProperty(proto, "foo",
		      { set: function() { sideEffect = 2; }, configurable: true });
g(obj1);
assertEq(sideEffect, 2);
sideEffect = undefined;
g(obj2);
assertEq(sideEffect, 2);
