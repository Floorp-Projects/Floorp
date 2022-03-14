function g1() {
    throw 10;
}

function g2() {
    throw 20;
}

class A {
    #x = "hello" + g1();
    constructor(o = g2()) {
    }
};

var thrown;
try {
    new A;
} catch (e) {
    thrown = e;
}

assertEq(thrown, 10);

if (typeof reportCompare === "function")
    reportCompare(true, true);
