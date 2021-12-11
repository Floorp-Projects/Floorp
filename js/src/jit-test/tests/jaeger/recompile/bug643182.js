x = 123;
function f() {}
function g(o) {
    y = x.p;
    eval('o');
}
g(f);
