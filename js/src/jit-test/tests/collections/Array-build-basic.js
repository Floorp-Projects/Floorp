// Array.build basics

load(libdir + "asserts.js");
load(libdir + "eqArrayHelper.js");

function myBuild(l, f) {
  var a = [];
  for (var i = 0; i < l; i++)
    a.push(f(i));
  return a;
}

// Test that build returns an identical, but new array.
var a1 = [];
for (var i = 0; i < 100; i++)
  a1[i] = Math.random();
var a2 = Array.build(a1.length, (i) => a1[i]);

assertEq(a1 === a2, false);
assertEqArray(a2, a1);
