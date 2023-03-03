function test() {
    // Some bound callables that we're unlikely to optimize better in CacheIR.
    var boundCtor = (new Proxy(Array, {})).bind(null, 1, 2, 3);
    var boundNonCtor = (new Proxy(x => x + 1, {})).bind(null, 1, 2, 3);

    for (var i = 0; i < 60; i++) {
        var fun = i < 40 ? boundCtor : boundNonCtor;
        var ex = null;
        try {
            var res = new fun(100, 101);
            assertEq(JSON.stringify(res), "[1,2,3,100,101]");
        } catch (e) {
            ex = e;
        }
        assertEq(ex === null, i < 40);
    }
}
test();
