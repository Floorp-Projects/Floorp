function f(x,y,z) {
    return x + y + z;
}

Array.prototype[1] = 10;

function g() {
    var arr = [1, ,3,4,5,6];

    for (var i = 0; i < 10; ++i) {
        assertEq(f.apply(null, arr), 14);
    }
}
g();

Object.prototype[1] = 20;

function h() {
    delete arguments[1];
    return f.apply(null, arguments);
}
assertEq(h(1,2,3), 24);

function i() {
    o = arguments;
    delete o[1];
    return f.apply(null, o);
}
assertEq(i(1,2,3), 24);
