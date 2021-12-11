function f() {
    var x = 0;
    for (var i = 1; i < 1e6; i++) {
        if (i > 0)
            x += Math.clz32(i);
    }
    return x;
}

function g() {
    var x = 0;
    for (var i = 1; i < 1e6; i++) {
        x += Math.clz32(i);
    }
    return x;
}

function h() {
    var x = 0;
    for (var i = 0; i < 1e6; i++) {
        x += Math.clz32(i);
    }
    return x;
}

function k() {
    var x = 0;
    for (var i = -1000; i < 1000; i++) {
        x += Math.clz32(i);
        x += Math.clz32(i + 0.5);
    }
    return x;
}

assertEq(f(), 13048543);
assertEq(g(), 13048543);
assertEq(h(), 13048575);
assertEq(k(), 46078);
