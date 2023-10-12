// |jit-test| --fast-warmup
let depth = 0;
function f1(a2, a3, a4, a5) {
    f2();
}
function f2() {
    if (depth++ > 100) {
        return;
    }
    f1(1, 2);
    assertEq(JSON.stringify(Array.from(f1.arguments)), "[1,2]");
}
f1(1, 2);
