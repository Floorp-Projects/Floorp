var wrapper = evaluate("({a: 15, b: {c: 42}})", {global: newGlobal({sameZoneAs: this})});

function test() {
    var i, error;
    try {
        for (i = 0; i < 150; i++) {
            assertEq(wrapper.b.c, 42);
            assertEq(wrapper.a, 15);

            if (i == 142) {
                // Next access to wrapper.b should throw.
                nukeCCW(wrapper);
            }
        }
    } catch (e) {
        error = e;
    }

    assertEq(error.message.includes("dead object"), true);
    assertEq(i, 143);
}

test();
