// |jit-test| --enable-experimental-fields

class C {
    x = 5;
}

c = new C();
assertEq(5, c.x);

class D {
    #y = 5;
}

d = new D();
assertEq(5, d.#y);

if (typeof reportCompare === "function")
  reportCompare(true, true);
