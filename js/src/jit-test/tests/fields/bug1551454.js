// |jit-test| --enable-experimental-fields

class C {
  1 = eval();
}
new C();

class D {
  1.5 = eval();
}
new D();

if (typeof reportCompare === "function")
  reportCompare(true, true);
