var IsRegExpObject = getSelfHostedValue("IsRegExpObject");
var IsCallable = getSelfHostedValue("IsCallable");
var NewArrayIterator = getSelfHostedValue("NewArrayIterator");
var GuardToArrayIterator = getSelfHostedValue("GuardToArrayIterator");

function array() {
    var a = [0];
    assertEq(Array.isArray(a), true);
    assertEq(typeof a, "object");
    assertEq(IsRegExpObject(a), false);
    assertEq(IsCallable(a), false);
}

function array2(x) {
    var a;
    if (x) {
        a = [0];
    } else {
        a = [1];
    }
    assertEq(Array.isArray(a), true);
    assertEq(typeof a, "object");
    assertEq(IsRegExpObject(a), false);
    assertEq(IsCallable(a), false);
}

function object() {
    var o = {a: 1};
    assertEq(Array.isArray(o), false);
    assertEq(typeof o, "object");
    assertEq(IsRegExpObject(o), false);
    assertEq(IsCallable(o), false);
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
    assertEq(IsRegExpObject(o), false);
    assertEq(IsCallable(o), false);
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
    assertEq(IsRegExpObject(o), false);
    assertEq(IsCallable(o), false);
}

function lambda() {
    function f() {}
    assertEq(Array.isArray(f), false);
    assertEq(typeof f, "function");
    assertEq(IsRegExpObject(f), false);
    assertEq(IsCallable(f), true);
}

function arrow() {
    var f = () => {};
    assertEq(Array.isArray(f), false);
    assertEq(typeof f, "function");
    assertEq(IsRegExpObject(f), false);
    assertEq(IsCallable(f), true);
}

function regexp() {
    var r = /a/;
    assertEq(Array.isArray(r), false);
    assertEq(typeof r, "object");
    assertEq(IsRegExpObject(r), true);
    assertEq(IsCallable(r), false);
}

function regexp2(x) {
    var a;
    if (x) {
        a = /a/;
    } else {
        a = /b/;
    }
    assertEq(Array.isArray(a), false);
    assertEq(typeof a, "object");
    assertEq(IsRegExpObject(a), true);
    assertEq(IsCallable(a), false);
}

function iterator() {
    var i = NewArrayIterator();
    assertEq(Array.isArray(i), false);
    assertEq(typeof i, "object");
    assertEq(IsRegExpObject(i), false);
    assertEq(IsCallable(i), false);
    assertEq(GuardToArrayIterator(i), i);
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
    regexp()
    regexp2(b);
    iterator();

    b = !b;
}
