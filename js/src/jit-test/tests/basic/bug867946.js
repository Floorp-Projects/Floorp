// |jit-test| error: InternalError: too much recursion
b = {};
b.__proto__ = evalcx("lazy");
function g() {
    g(b.toString())
}
g();
