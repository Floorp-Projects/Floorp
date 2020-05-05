// |jit-test| --warp
// A simple test to ensure WarpBuilder files are included in code-coverage builds.
// See bug 1635097.

function test() {
    var o = {x: 0};
    for (var i = 0; i < 10000; i++) {
        o.x++;
    }
    return o;
}
test();
