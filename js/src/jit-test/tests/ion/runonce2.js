// Test that closures are deoptimized if they unexpectedly run multiple times.

with({}){}

(function(n) {
  if (n == 20) {
    outer = (function f() { return f.caller; })();
    inner = function g() { return x++; }
  }
  var x = 0;
  for (var i = 0; i < n; i++)
    x++;
  if (n != 20)
    inner = function g() { return x++; }
})(20);

oldInner = inner;

for (var i = 0; i < 5; i++)
  assertEq(oldInner(), 20 + i);

outer(40);

for (var i = 0; i < 5; i++)
  assertEq(inner(), 40 + i);

for (var i = 0; i < 5; i++)
  assertEq(oldInner(), 25 + i);
