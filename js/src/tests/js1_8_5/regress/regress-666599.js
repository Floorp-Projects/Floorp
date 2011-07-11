var o1 = {};
var o2 = new ArrayBuffer();
o2.__defineGetter__('x', function() {return 42;})
o1.__proto__ = o2
assertEq(o1['x'], 42);

var t1 = {};
var t2 = new ArrayBuffer();
var t3 = new ArrayBuffer();
t3.__defineGetter__('x', function() {return 42;})
t2.__proto__ = t3;
t1.__proto__ = t2;
assertEq(t1['x'], 42);

reportCompare(true, true);
