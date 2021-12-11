
with({}){}
x = new Int8Array(1)
function f(y) {
    x[0] = y
}
f()
f(3)
f(7)
x.buffer;
f(0);
assertEq(x[0], 0);
