// |jit-test| --enable-experimental-fields

load(libdir + "eqArrayHelper.js");

let expected = [];
class B {
    constructor(...args) {
        assertEqArray(expected, args);
    }
}

class C extends B {
    asdf = 2;
}

expected = [];
new C();
expected = [1];
new C(1);
expected = [1, 2];
new C(1, 2);

if (typeof reportCompare === "function")
  reportCompare(true, true);
