// don't assert or crash
function g(o) {
    o.__proto__ = arguments;
    o.length = 123;
}
function f() {
    g(arguments);
}
f();

