// |jit-test| --enable-experimental-fields

class C {
    x;
    y(){}
    z = 2;
    w(){};
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
