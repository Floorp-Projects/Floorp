gczeal(2);
try {
    DoWhile_3();
} catch (e) {}
function f() {
    test();
    yield 170;
}
function test() {
    function foopy() {
        try {
            for (var i in f());
        } catch (e) {}
    }
    foopy();
    gc();
}
test();
