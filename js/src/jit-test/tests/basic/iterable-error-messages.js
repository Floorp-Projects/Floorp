function assertThrowsMsg(f, msg) {
    try {
        f();
        assertEq(0, 1);
    } catch(e) {
        assertEq(e instanceof TypeError, true);
        assertEq(e.message, msg);
    }
}

// For-of
function testForOf(val) {
    for (var x of val) {}
}
for (v of [{}, Math, new Proxy({}, {})]) {
    assertThrowsMsg(() => testForOf(v), "val is not iterable");
}
assertThrowsMsg(() => testForOf(null), "val is null");
assertThrowsMsg(() => { for (var x of () => 1) {}}, "() => 1 is not iterable");

// Destructuring
function testDestr(val) {
    var [a, b] = val;
}
for (v of [{}, Math, new Proxy({}, {})]) {
    assertThrowsMsg(() => testDestr(v), "val is not iterable");
}
assertThrowsMsg(() => testDestr(null), "val is null");
assertThrowsMsg(() => { [a, b] = () => 1; }, "() => 1 is not iterable");

// Spread
function testSpread(val) {
    [...val];
}
for (v of [{}, Math, new Proxy({}, {})]) {
    assertThrowsMsg(() => testSpread(v), "val is not iterable");
}
assertThrowsMsg(() => testSpread(null), "val is null");
assertThrowsMsg(() => { [...() => 1]; }, "() => 1 is not iterable");
