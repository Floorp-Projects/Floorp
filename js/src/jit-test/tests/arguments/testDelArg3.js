function assertGood(x) {
    assertEq(x, "good");
}

(function() {
    var a = arguments;
    return function() {
        assertGood.apply(null, a);
    }
})("good")();

(function() {
    var a = arguments;
    return function() {
        a[0] = "good";
        assertGood.apply(null, a);
    }
})("bad")();

Object.prototype[0] = "good";

(function() {
    var a = arguments;
    return function() {
        delete a[0];
        assertGood.apply(null, a);
    }
})("bad")();

delete Object.prototype[0];

function assertUndefined(x) {
    assertEq(x, undefined);
}

(function() {
    var a = arguments;
    return function() {
        a[0] = "bad";
        assertUndefined.apply(null, a);
    }
})()();
