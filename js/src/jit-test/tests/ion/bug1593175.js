function f() {
    f;
}
f();
f();
function g() {
    typeof(f = []) + f > 2;
}
g();
g();
