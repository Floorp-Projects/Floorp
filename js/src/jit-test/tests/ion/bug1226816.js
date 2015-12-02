// |jit-test| error: InternalError

x = 1;
x;
function g(y) {}
g(this);
x = /x/;
function f() {
    f(x.flags);
}
f();
