
function g() {
    yield
}
g()
function f() {
    g()
}
try {
    new f
} catch (e) {}
