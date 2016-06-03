// |jit-test| error: unreachable code
function f() {
    return 1;
    return 2;
}
options("werror");
f();
