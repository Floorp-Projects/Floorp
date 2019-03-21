// |jit-test| --enable-experimental-fields

class C {
    x;;;;
    y
    ;
}

class D {
    x = 5;
    y = (this.x += 1) + 2;
}

let val = new D();
assertEq(6, val.x);
assertEq(8, val.y);

if (typeof reportCompare === "function")
  reportCompare(true, true);
