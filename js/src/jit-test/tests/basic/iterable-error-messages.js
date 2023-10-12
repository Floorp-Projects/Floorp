// |jit-test| skip-if: getBuildConfiguration('pbl')

function assertThrowsMsgEndsWith(f, msg) {
    try {
        f();
        assertEq(0, 1);
    } catch(e) {
        assertEq(e instanceof TypeError, true);
        assertEq(e.message.endsWith(msg), true);
    }
}

// For-of
function testForOf(val) {
    for (var x of val) {}
}
for (v of [{}, Math, new Proxy({}, {})]) {
    assertThrowsMsgEndsWith(() => testForOf(v), "val is not iterable");
}
assertThrowsMsgEndsWith(() => testForOf(null), "val is null");
assertThrowsMsgEndsWith(() => { for (var x of () => 1) {}}, "() => 1 is not iterable");

// Destructuring
function testDestr(val) {
    var [a, b] = val;
}
for (v of [{}, Math, new Proxy({}, {})]) {
    assertThrowsMsgEndsWith(() => testDestr(v), "val is not iterable");
}
assertThrowsMsgEndsWith(() => testDestr(null), "val is null");
assertThrowsMsgEndsWith(() => { [a, b] = () => 1; }, "() => 1 is not iterable");

// Spread
function testSpread(val) {
    [...val];
}
for (v of [{}, Math, new Proxy({}, {})]) {
    assertThrowsMsgEndsWith(() => testSpread(v), "val is not iterable");
}
assertThrowsMsgEndsWith(() => testSpread(null), "val is null");
assertThrowsMsgEndsWith(() => { [...() => 1]; }, "() => 1 is not iterable");
