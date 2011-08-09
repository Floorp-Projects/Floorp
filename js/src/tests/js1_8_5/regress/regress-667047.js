var x = new ArrayBuffer();
// first set to null should make the proto chain undefined
x.__proto__ = null;
assertEq(x.__proto__, undefined);
// second set makes it a property
x.__proto__ = null;
assertEq(x.__proto__, null);
// always act as a property now
x.__proto__ = {a:2};
assertEq(x.__proto__.a, 2);
assertEq(x.a, undefined);

var ab = new ArrayBuffer();
// not the same as setting __proto__ to null
ab.__proto__ = Object.create(null);
ab.__proto__ = {a:2};
// should still act like __proto__ is a plain property
assertEq(ab.a, undefined);
reportCompare(true, true);
