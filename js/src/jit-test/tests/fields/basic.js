// |jit-test| --enable-experimental-fields

class C {
    x;
    y = 2;
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
