
x = [];
x[0] = 2;
x[1] = 2;

function f() {
    eval("function r() { arguments }");
};

function g() {
    for (var i = 0; i < x.length; ++i) {
        f();
    }
}

g();
