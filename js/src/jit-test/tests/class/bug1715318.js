// |jit-test| --no-threads; --fast-warmup
function f() {
    // A non-constructor with a null .prototype.
    let arrow = _ => null;
    arrow.prototype = null;

    // Warm up.
    for (var i = 0; i < 100; i++) {}

    try {
        class C1 extends arrow {}
        throw "Fail";
    } catch (e) {
        assertEq(e instanceof TypeError, true);
    }
}
f();
f();
