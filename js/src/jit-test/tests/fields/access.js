class C {
    x = 5;
}

c = new C();
assertEq(5, c.x);

if (typeof reportCompare === "function")
  reportCompare(true, true);
