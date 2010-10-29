// Try to test that we handle shape regeneration correctly.
// This is a fragile test, but as of this writing, on dmandelin's
// windows box, we have the same shape number with different
// logical shapes in the two assertEq lines.

var o;
var p;
var zz;
var o2;

function f(x) {
  return x.a;
}

gczeal(1);
gc();

zz = { q: 11 };
o = { a: 77, b: 88 };
o2 = { c: 11 };
p = { b: 99, a: 11 };

//print('s ' + shapeOf(zz) + ' ' + shapeOf(o) + ' ' + shapeOf(o2) + ' ' + shapeOf(p));

assertEq(f(o), 77);

o = undefined;

gczeal(1);
gc();
//print('s ' + 'x' + ' ' + shapeOf(p));

assertEq(f(p), 11);
