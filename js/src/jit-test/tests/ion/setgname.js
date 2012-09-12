
// aaa is initially undefined. Make sure it's set to the
// correct value - we have to store the type tag, even though
// its known type is int32.
var aaa;
function f() {
    function g(x) {
        if (x)
            aaa = 22;
    }
    g(10);

    function h() {
        aaa = 22;
    }
    for (var i=0; i<70; i++) {
        h();
    }
    assertEq(aaa, 22);
}
f();

x = 0;
function setX(i) {
    x = i;
}
for (var i=0; i<70; i++)
    setX(i);
assertEq(x, 69);

y = 3.14;
y = true;
y = [];
function setY(arg) {
    y = arg;
}
for (var i=0; i<70; i++)
    setY([1]);
setY([1, 2, 3]);
assertEq(y.length, 3);

// z is non-configurable, but can be made non-writable.
var z = 10;

function testNonWritable() {
    function g() {
        z = 11;
    }
    for (var i=0; i<70; i++) {
        g();
    }
    Object.defineProperty(this, "z", {value: 1234, writable: false});
    g();
    assertEq(z, 1234);
}
testNonWritable();
