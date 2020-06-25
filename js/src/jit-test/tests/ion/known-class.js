function array() {
    var a = [0];
    assertEq(Array.isArray(a), true);
    assertEq(typeof a, "object");
}

function array2(x) {
    var a;
    if (x) {
        a = [0]
    } else {
        a = [1];
    }
    assertEq(Array.isArray(a), true);
    assertEq(typeof a, "object");
}

function object() {
    var o = {a: 1};
    assertEq(Array.isArray(o), false);
    assertEq(typeof o, "object");
}

function object2(x) {
    var o;
    if (x) {
        o = {a: 1};
    } else {
        o = {b: 1};
    }
    assertEq(Array.isArray(o), false);
    assertEq(typeof o, "object");
}

function mixed(x) {
    var o;
    if (x) {
        o = [1];
    } else {
        o = {a: 1};
    }
    assertEq(Array.isArray(o), x);
    assertEq(typeof o, "object");
}

function lambda() {
    function f() {}
    assertEq(Array.isArray(f), false);
    assertEq(typeof f, "function");
}

function arrow() {
    var f = () => {};
    assertEq(Array.isArray(f), false);
    assertEq(typeof f, "function");
}


var b = true;
for (var i = 0; i < 1e4; i++) {
    array();
    array2(b);
    object();
    object2(b);
    mixed(b);
    lambda();
    arrow();

    b = !b;
}
