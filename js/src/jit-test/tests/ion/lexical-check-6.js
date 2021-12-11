// This function uses UCE to test when the if branch is removed by
// IonMonkey.  Some optimization such as Scalar Replacement are able to remove
// the scope chain, which can cause issues when the scope chain properties are
// not initialized properly.
var uceFault = function (i) {
    if (i % 1500 == 0) {
        uceFault = function (i) { return i % 1500 == 0; };
    }
    return false;
};

function f(i) {
    if (uceFault(i) || uceFault(i))
        g();
    const x = 42;
    function g() {
        return x;
    }
    return g;
}

function loop() {
    for (; i < 4000; i++)
        assertEq(f(i)(), 42);
}

var caught = 0;
var i = 1;
while (i < 4000) {
    try {
        loop();
    } catch(e) {
        assertEq(e instanceof ReferenceError, true);
        assertEq(i == 1500 || i == 3000, true);
        caught += 1;
        i++;
    }
}
assertEq(caught, 2);
