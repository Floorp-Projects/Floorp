// |jit-test| --fast-warmup

// Tests for exception bailout from inlined getter/setter.

function throwingGetter() {
    var o = {};
    var count = 0;
    Object.defineProperty(o, "getter", {get: function() {
        if (count++ === 195) {
            throw 1;
        }
    }});
    var ex = null;
    try {
        for (var i = 0; i < 200; i++) {
            o.getter;
        }
    } catch(e) {
        ex = e;
    }
    assertEq(ex, 1);
    assertEq(count, 196);
}
throwingGetter();

function throwingSetter() {
    var o = {};
    var count = 0;
    Object.defineProperty(o, "setter", {set: function(v) {
        if (count++ === 195) {
            throw 1;
        }
    }});
    var ex = null;
    try {
        for (var i = 0; i < 200; i++) {
            o.setter = i;
        }
    } catch(e) {
        ex = e;
    }
    assertEq(ex, 1);
    assertEq(count, 196);
}
throwingSetter();
