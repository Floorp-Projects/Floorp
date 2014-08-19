// ToString(symbol) throws a TypeError.

if (typeof Symbol === "function") {
    var N = 10, obj, hits = 0;
    for (var i = 0; i < N; i++) {
        try {
            obj = new String(Symbol());
        } catch (exc) {
            hits++;
        }
    }
    assertEq(hits, N);
}
