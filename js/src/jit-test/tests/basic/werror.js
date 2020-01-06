// |jit-test| error: unreachable code; skip-if: isLcovEnabled()

function f() {
    return 1;
    return 2;
}
options("werror");
f();
