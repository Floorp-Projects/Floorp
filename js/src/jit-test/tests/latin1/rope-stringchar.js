function f(a, b) {
    assertEq(isLatin1(a), true);
    assertEq(isLatin1(b), false);
    var c = a + b;
    assertEq(isLatin1(c), false);
    assertEq(c[4], 'b');
    assertEq(c.charCodeAt(4), 98);
}

function test() {
    for (var i = 0; i < 15; i++) {
        f("aaaab\x00\x00\x00\x00\x00\x00", "\u{2603}");
    }
}

test();
