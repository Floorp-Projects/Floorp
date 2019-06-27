let f = 1;
class X { f=f; }
assertEq(new X().f, 1);

if (typeof reportCompare === "function")
  reportCompare(true, true);
