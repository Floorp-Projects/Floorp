function f(x = let (x) 1, x = 4) { return x; }
assertEq(f(), 4);
