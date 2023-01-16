// |jit-test| --ion-warmup-threshold=20

function testBailout() {
    function f(v, r) {
        for (var i = 0; i < 50; ++i) {
            // Ensure DCE and LICM don't eliminate calls to fromCodePoint in
            // case the input argument is not a valid code point.
            if (i === 0) {
                r();
            }
            String.fromCodePoint(v);
            String.fromCodePoint(v);
            String.fromCodePoint(v);
        }
    }

    var result = [];
    function r() {
        result.push("ok");
    }

    do {
        result.length = 0;
        try {
            f(0, r);
            f(0, r);
            f(0x10ffff + 1, r);
        } catch (e) {}
        assertEq(result.length, 3);
    } while (!inIon());
}
testBailout();
