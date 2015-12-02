function f() {
    var hits = 0;
    for (var T of [Float32Array, Float64Array, Float32Array]) {
        var arr = new T(1);
        try {
            arr[0] = Symbol.iterator;
        } catch(e) { hits++; }
    }
    for (var T of [Int32Array, Int16Array, Int8Array]) {
        var arr = new T(1);
        try {
            arr[0] = Symbol.iterator;
        } catch(e) { hits++; }
    }
    assertEq(hits, 6);
}
f();
